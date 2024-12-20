// so.c
// sistema operacional
// simulador de computador
// so24b

// INCLUDES {{{1
#include "so.h"
#include "dispositivos.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"
#include "processo.h"
#include "escalonador.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

// CONSTANTES E TIPOS {{{1
// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas

#define PROC_TAM_TABELA 1000
#define MAX_QUANTUM 5

#define ESC_TIPO ESC_PRIORIDADE

typedef struct so_metricas_t {
    int t_exec;
    int t_ocioso;
    int n_preempcoes;
    int n_irq[N_IRQ];
} so_metricas_t;

struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  es_t *es;
  console_t *console;
  bool erro_interno;
  // t1: tabela de processos, processo corrente, pendências, etc
  processo_t **proc_tabela;
  processo_t *proc_atual;
  int proc_ultimo_id;
  
  esc_t *esc;
  
  int t_relogio_atual;
  
  historico_t *hist;
  so_metricas_t metricas;
};


// função de tratamento de interrupção (entrada no SO)
static int so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
// carrega o programa contido no arquivo na memória do processador; retorna end. inicial
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
// copia para str da memória do processador, até copiar um 0 (retorna true) ou tam bytes
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);

// CRIAÇÃO {{{1

so_t *so_cria(cpu_t *cpu, mem_t *mem, es_t *es, console_t *console)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  self->cpu = cpu;
  self->mem = mem;
  self->es = es;
  self->console = console;
  self->erro_interno = false;
  
  // cria tabela de processos
  self->proc_tabela = (processo_t**)malloc(sizeof(processo_t*) * PROC_TAM_TABELA);
  self->proc_atual = NULL;
  self->proc_ultimo_id = 0;

  // cria escalonador
  self->esc = escalonador_cria(ESC_TIPO, PROC_TAM_TABELA, MAX_QUANTUM);

  // cria histórico
  self->hist = NULL;

  // cria métricas
  self->metricas.t_exec = 0;
  self->metricas.t_ocioso = 0;
  self->metricas.n_preempcoes = 0;

  // quando a CPU executar uma instrução CHAMAC, deve chamar a função
  //   so_trata_interrupcao, com primeiro argumento um ptr para o SO
  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);

  // coloca o tratador de interrupção na memória
  // quando a CPU aceita uma interrupção, passa para modo supervisor, 
  //   salva seu estado à partir do endereço 0, e desvia para o endereço
  //   IRQ_END_TRATADOR
  // colocamos no endereço IRQ_END_TRATADOR o programa de tratamento
  //   de interrupção (escrito em asm). esse programa deve conter a 
  //   instrução CHAMAC, que vai chamar so_trata_interrupcao (como
  //   foi definido acima)
  int ender = so_carrega_programa(self, "trata_int.maq");
  if (ender != IRQ_END_TRATADOR) {
    console_printf("SO: problema na carga do programa de tratamento de interrupção");
    self->erro_interno = true;
  }

  // programa o relógio para gerar uma interrupção após INTERVALO_INTERRUPCAO
  if (es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO) != ERR_OK) {
    console_printf("SO: problema na programação do timer");
    self->erro_interno = true;
  }

  console_printf("criando");

  return self;
}

void so_destroi(so_t *self)
{
  cpu_define_chamaC(self->cpu, NULL, NULL);
  escalonador_destroi(self->esc);
  historico_apaga(self->hist);
  free(self->proc_tabela);
  free(self);
}

// PROCESSOS {{{1
static processo_t *so_cria_processo(so_t *self, char *origem) {
    console_printf("Criando processo");
    int ender = so_carrega_programa(self, origem);
    processo_t *p = processo_cria(self->proc_ultimo_id + 1, ender, MAX_QUANTUM);
    if (p == NULL) return NULL;
    for (int i = 0; i < PROC_TAM_TABELA; i++) {
        if (self->proc_tabela[i] == NULL) {
            self->proc_tabela[i] = p;
            break;
        }
    }
    escalonador_adiciona_processo(self->esc, p);
    self->proc_ultimo_id++;
    return p;
}

static int so_mata_processo(so_t *self, int id) {
    escalonador_remove_processo(self->esc, 0);
    int index = -1;
    for (int i = 0; i < PROC_TAM_TABELA; i++) {
        if (id == 0 && self->proc_atual == self->proc_tabela[i]) {
            self->proc_atual = NULL;
            index = i;
            break;
        }
        else if (id == processo_pega_id(self->proc_tabela[i])) {
            index = i;
            break;
        }
    }
    if (index == -1) return -1;
    self->hist = historico_adiciona_metrica(self->hist, self->proc_tabela[index]);
    processo_mata(self->proc_tabela[index]);
    self->proc_tabela[index] = NULL;
    return 0;
}

static void so_bloqueia_processo(so_t *self, proc_bloqueio_t motivo) {
    processo_bloqueia(self->proc_atual, motivo);
    escalonador_remove_processo(self->esc, 0);
    console_printf("Processo %d bloqueado", processo_pega_id(self->proc_atual));
}

static void so_desbloqueia_processo(so_t *self, processo_t *proc) {
    processo_muda_estado(proc, PROC_PRONTO);
    escalonador_adiciona_processo(self->esc, proc);
    console_printf("Processo desbloqueado");
}
// MÉTRICAS {{{1
static void so_atualiza_metricas(so_t *self, int preemp) {
    int t_relogio_anterior = self->t_relogio_atual;
    if (es_le(self->es, D_RELOGIO_INSTRUCOES, &self->t_relogio_atual) != ERR_OK) {
        console_printf("SO: problema na leitura do relógio");
        return;
    }
    if (t_relogio_anterior == -1) return;
    int delta = self->t_relogio_atual - t_relogio_anterior;
    self->metricas.t_exec += delta;
    if (self->proc_atual == NULL) {
        self->metricas.t_ocioso += delta;
    }
    self->metricas.n_preempcoes += preemp;
    for (int i = 0; i < PROC_TAM_TABELA; i++) {
        processo_t *p = self->proc_tabela[i];
        if (p != NULL) {
            processo_atualiza_metricas(p, delta);
        }
    }
}

static void so_imprime_metricas(so_t *self) {
    console_printf("Tempo execução: %d", self->metricas.t_exec);
    console_printf("Tempo ocioso:   %d", self->metricas.t_ocioso);
    console_printf("N. preempções:  %d", self->metricas.n_preempcoes);

    for (int i = 0; i < N_IRQ; i++) {
        console_printf("N. interrup. %d: %d", i, self->metricas.n_irq[i]);
    }
    /*
    for (historico_t *h = self->hist; h != NULL; h = historico_prox(h)) {
        proc_metricas_t metricas = historico_pega_metricas(h);
        console_printf("\nID: %d", historico_pega_id(id));
        console_printf("N. preempções: %d", metricas.n_preempcoes);
        console_printf("T. retorno:    %d", metricas.tempo_retorno);
        console_printf("T. resposta:   %d", metricas.tempo_resposta);
        for (int i = 0; i < N_PROC_ESTADO; i++) {
            console_printf("Vezes em %d: %d", i, metricas.estado_vezes[i]);
            console_printf("Tempo em %d: %d", i, metricas.estado_tempo[i]);
        }
    }
    */
}

static void so_imprime_metricas_arquivo(so_t *self) {
    char nome[100];
    sprintf(nome, "../Metricas/log_%d_%d_%d.txt", ESC_TIPO, INTERVALO_INTERRUPCAO, MAX_QUANTUM);
    FILE *log = fopen(nome, "w");
    fprintf(log, "Tempo execução: %d\n", self->metricas.t_exec);
    fprintf(log, "Tempo ocioso:   %d\n", self->metricas.t_ocioso);
    fprintf(log, "N. preempções:  %d\n", self->metricas.n_preempcoes);

    for (int i = 0; i < N_IRQ; i++) {
        fprintf(log, "N. interrup. %d: %d\n", i, self->metricas.n_irq[i]);
    }
    for (historico_t *h = self->hist; h != NULL; h = historico_prox(h)) {
        proc_metricas_t metricas = historico_pega_metricas(h);
        fprintf(log, "\nID: %d\n", historico_pega_id(h));
        fprintf(log, "N. preempções: %d\n", metricas.n_preempcoes);
        fprintf(log, "T. retorno:    %d\n", metricas.tempo_retorno);
        fprintf(log, "T. resposta:   %d\n", metricas.tempo_resposta);
        for (int i = 0; i < N_PROC_ESTADO; i++) {
            fprintf(log, "Vezes em %d: %d\n", i, metricas.estado_vezes[i]);
            fprintf(log, "Tempo em %d: %d\n", i, metricas.estado_tempo[i]);
        }
    }
}

// TRATAMENTO DE BLOQUEIO {{{1

static int so_trata_bloq_espera(so_t *self, processo_t *p) {
  int pid = processo_pega_reg_x(p);
  if (pid == processo_pega_id(p)) {
      so_desbloqueia_processo(self, p);
      return 1;
  }
  for (int i = 0; i < PROC_TAM_TABELA; i++) {
      if (pid == processo_pega_id(self->proc_tabela[i])) {
          return 0;
      }
  }
  so_desbloqueia_processo(self, p);
  return 1;
}

static int so_trata_bloq_entrada(so_t *self, processo_t *p) {
    int term = (processo_pega_id(p) - 1) % 4 * 4;
    int estado;
    if (es_le(self->es, D_TERM_A_TECLADO_OK + term, &estado) != ERR_OK) {
        self->erro_interno = true;
        return 0;
    }
    if (estado == 0) {
        return 0;
    }
    int dado;
    if (es_le(self->es, D_TERM_A_TECLADO + term, &dado) != ERR_OK) {
        self->erro_interno = true;
        return 0;
    }
    processo_salva_reg_a(p, dado);
    so_desbloqueia_processo(self, p);
    return 1;
}

static int so_trata_bloq_saida(so_t *self, processo_t *p) {
    int term = (processo_pega_id(p) - 1) % 4 * 4;
    int estado;
    if (es_le(self->es, D_TERM_A_TELA_OK + term, &estado) != ERR_OK) {
        self->erro_interno = true;
        return 0;
    }
    if (estado == 0) {
        return 0;
    }
    int dado = processo_pega_reg_x(p);
    if (es_escreve(self->es, D_TERM_A_TELA + term, dado) != ERR_OK) {
        self->erro_interno = true;
        return 0;
    }
    processo_salva_reg_a(p, 0);
    so_desbloqueia_processo(self, p);
    return 1;
}

// TRATAMENTO DE INTERRUPÇÃO {{{1

// funções auxiliares para o tratamento de interrupção
static void so_salva_estado_da_cpu(so_t *self);
static void so_trata_irq(so_t *self, int irq);
static void so_trata_pendencias(so_t *self);
static void so_escalona(so_t *self);
static int so_despacha(so_t *self);
static int so_desliga(so_t *self);

// função a ser chamada pela CPU quando executa a instrução CHAMAC, no tratador de
//   interrupção em assembly
// essa é a única forma de entrada no SO depois da inicialização
// na inicialização do SO, a CPU foi programada para chamar esta função para executar
//   a instrução CHAMAC
// a instrução CHAMAC só deve ser executada pelo tratador de interrupção
//
// o primeiro argumento é um ponteiro para o SO, o segundo é a identificação
//   da interrupção
// o valor retornado por esta função é colocado no registrador A, e pode ser
//   testado pelo código que está após o CHAMAC. No tratador de interrupção em
//   assembly esse valor é usado para decidir se a CPU deve retornar da interrupção
//   (e executar o código de usuário) ou executar PARA e ficar suspensa até receber
//   outra interrupção
static int so_trata_interrupcao(void *argC, int reg_A)
{
  console_printf("teste");
  so_t *self = argC;
  irq_t irq = reg_A;
  // esse print polui bastante, recomendo tirar quando estiver com mais confiança
  console_printf("SO: recebi IRQ %d (%s)", irq, irq_nome(irq));
  // salva o estado da cpu no descritor do processo que foi interrompido
  so_salva_estado_da_cpu(self);
  // faz o atendimento da interrupção
  so_trata_irq(self, irq);
  // faz o processamento independente da interrupção
  so_trata_pendencias(self);
  // escolhe o próximo processo a executar
  so_escalona(self);
  // recupera o estado do processo escolhido
  if (self->proc_tabela[0] != NULL) {
      return so_despacha(self);
  }
  else {
      return so_desliga(self);
  }
}

static void so_salva_estado_da_cpu(so_t *self)
{
  // t1: salva os registradores que compõem o estado da cpu no descritor do
  //   processo corrente. os valores dos registradores foram colocados pela
  //   CPU na memória, nos endereços IRQ_END_*
  // se não houver processo corrente, não faz nada
  if (self->proc_atual == NULL) return;
  if (processo_pega_estado(self->proc_atual) != PROC_EXECUTANDO) return;
  int pc, a, x;
  mem_le(self->mem, IRQ_END_PC, &pc);
  mem_le(self->mem, IRQ_END_A, &a);
  mem_le(self->mem, IRQ_END_X, &x);
  processo_salva_reg_pc(self->proc_atual, pc);
  processo_salva_reg_a(self->proc_atual, a);
  processo_salva_reg_x(self->proc_atual, x);
}

static void so_trata_pendencias(so_t *self)
{
    for (int i = 0; i < PROC_TAM_TABELA; i++) {
        processo_t *p = self->proc_tabela[i];
        if (p != NULL && processo_pega_estado(p) == PROC_BLOQUEADO) {
            switch (processo_pega_bloq_motivo(p)) {
                case PROC_BLOQ_ESPERA:
                so_trata_bloq_espera(self, p);
                break;
                case PROC_BLOQ_ENTRADA:
                so_trata_bloq_entrada(self, p);
                break;
                case PROC_BLOQ_SAIDA:
                so_trata_bloq_saida(self, p);
                break;
                default:
                break;
            }
        }
    }
}

static void so_escalona(so_t *self)
{
  // escolhe o próximo processo a executar, que passa a ser o processo
  //   corrente; pode continuar sendo o mesmo de antes ou não
  // t1: na primeira versão, escolhe um processo caso o processo corrente não possa continuar
  //   executando. depois, implementar escalonador melhor
    int preemp = 0;
    console_printf("Escalonando agora");
    escalonador_escalona(self->esc, self);
    self->proc_atual = escalonador_pega_atual(self->esc);
    console_printf("PROC ATUAL: %d %d", processo_pega_id(self->proc_atual), processo_pega_estado(self->proc_atual));
    so_atualiza_metricas(self, preemp);
}

static int so_despacha(so_t *self)
{
  // t1: se houver processo corrente, coloca o estado desse processo onde ele
  //   será recuperado pela CPU (em IRQ_END_*) e retorna 0, senão retorna 1
  // o valor retornado será o valor de retorno de CHAMAC
  if (self->erro_interno) return 1;
  if (self->proc_atual == NULL) return 1;
  if (processo_pega_estado(self->proc_atual) != PROC_EXECUTANDO) return 1;
  int pc, a, x;
  pc = processo_pega_reg_pc(self->proc_atual);
  a = processo_pega_reg_a(self->proc_atual);
  x = processo_pega_reg_x(self->proc_atual);
  mem_escreve(self->mem, IRQ_END_PC, pc);
  mem_escreve(self->mem, IRQ_END_A, a);
  mem_escreve(self->mem, IRQ_END_X, x);
  console_printf("PC: %d", pc);
  return 0;
}

int so_desliga(so_t *self) {
    if (es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0) != ERR_OK) {
        console_printf("SO: Problema de desarme do timer");
        self->erro_interno = true;
    }
    if (es_escreve(self->es, D_RELOGIO_TIMER, 0) != ERR_OK) {
        console_printf("SO: Problema de desarme do timer");
        self->erro_interno = true;
    }

    so_imprime_metricas(self);
    so_imprime_metricas_arquivo(self);
    return 1;
}

// TRATAMENTO DE UMA IRQ {{{1

// funções auxiliares para tratar cada tipo de interrupção
static void so_trata_irq_reset(so_t *self);
static void so_trata_irq_chamada_sistema(so_t *self);
static void so_trata_irq_err_cpu(so_t *self);
static void so_trata_irq_relogio(so_t *self);
static void so_trata_irq_desconhecida(so_t *self, int irq);

static void so_trata_irq(so_t *self, int irq)
{
  // verifica o tipo de interrupção que está acontecendo, e atende de acordo
  self->metricas.n_irq[irq]++;
  switch (irq) {
    case IRQ_RESET:
      so_trata_irq_reset(self);
      break;
    case IRQ_SISTEMA:
      so_trata_irq_chamada_sistema(self);
      break;
    case IRQ_ERR_CPU:
      so_trata_irq_err_cpu(self);
      break;
    case IRQ_RELOGIO:
      so_trata_irq_relogio(self);
      break;
    default:
      so_trata_irq_desconhecida(self, irq);
  }
}

// interrupção gerada uma única vez, quando a CPU inicializa
static void so_trata_irq_reset(so_t *self)
{
  // t1: deveria criar um processo para o init, e inicializar o estado do
  //   processador para esse processo com os registradores zerados, exceto
  //   o PC e o modo.
  // como não tem suporte a processos, está carregando os valores dos
  //   registradores diretamente para a memória, de onde a CPU vai carregar
  //   para os seus registradores quando executar a instrução RETI

  // coloca o programa init na memória
  self->proc_atual = so_cria_processo(self, "init.maq");
  if (self->proc_atual == NULL || processo_pega_reg_pc(self->proc_atual) != 100) {
    console_printf("SO: problema na carga do programa inicial");
    self->erro_interno = true;
    return;
  }

  // altera o PC para o endereço de carga
  mem_escreve(self->mem, IRQ_END_PC, processo_pega_reg_pc(self->proc_atual));
  // passa o processador para modo usuário
  mem_escreve(self->mem, IRQ_END_modo, usuario);
}

// interrupção gerada quando a CPU identifica um erro
static void so_trata_irq_err_cpu(so_t *self)
{
  // Ocorreu um erro interno na CPU
  // O erro está codificado em IRQ_END_erro
  // Em geral, causa a morte do processo que causou o erro
  // Ainda não temos processos, causa a parada da CPU
  int err_int;
  // t1: com suporte a processos, deveria pegar o valor do registrador erro
  //   no descritor do processo corrente, e reagir de acordo com esse erro
  //   (em geral, matando o processo)
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int;
  console_printf("SO: IRQ erro na CPU: %s", err_nome(err));
  if(err == ERR_OK) return;
  console_printf("Matando processo");
  if (self->proc_atual == NULL) return;
  if (so_mata_processo(self, 0) == 0) {
      mem_escreve(self->mem, IRQ_END_A, 0);
      console_printf("Morto processo");
  }
  else {
      mem_escreve(self->mem, IRQ_END_A, -1);
  }
}

// interrupção gerada quando o timer expira
static void so_trata_irq_relogio(so_t *self)
{
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0); // desliga o sinalizador de interrupção
  e2 = es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO);
  if (e1 != ERR_OK || e2 != ERR_OK) {
    console_printf("SO: problema da reinicialização do timer");
    self->erro_interno = true;
  }
  // t1: deveria tratar a interrupção
  //   por exemplo, decrementa o quantum do processo corrente, quando se tem
  //   um escalonador com quantum
  processo_decrementa_quantum(self->proc_atual);
  console_printf("Quantum p_atual: %d", processo_pega_quantum(self->proc_atual));
}

// foi gerada uma interrupção para a qual o SO não está preparado
static void so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf("SO: não sei tratar IRQ %d (%s)", irq, irq_nome(irq));
  self->erro_interno = true;
}

// CHAMADAS DE SISTEMA {{{1

// funções auxiliares para cada chamada de sistema
static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);
static void so_chamada_espera_proc(so_t *self);

static void so_trata_irq_chamada_sistema(so_t *self)
{
  // a identificação da chamada está no registrador A
  // t1: com processos, o reg A tá no descritor do processo corrente
  int id_chamada;
  if (self->proc_atual == NULL) {
    console_printf("SO: erro no acesso ao id da chamada de sistema");
    self->erro_interno = true;
    return;
  }
  id_chamada = processo_pega_reg_a(self->proc_atual);
  console_printf("SO: chamada de sistema %d", id_chamada);
  switch (id_chamada) {
    case SO_LE:
      so_chamada_le(self);
      break;
    case SO_ESCR:
      so_chamada_escr(self);
      break;
    case SO_CRIA_PROC:
      so_chamada_cria_proc(self);
      break;
    case SO_MATA_PROC:
      so_chamada_mata_proc(self);
      break;
    case SO_ESPERA_PROC:
      so_chamada_espera_proc(self);
      break;
    default:
      console_printf("SO: chamada de sistema desconhecida (%d)", id_chamada);
      // t1: deveria matar o processo
      self->erro_interno = true;
  }
}

// implementação da chamada se sistema SO_LE
// faz a leitura de um dado da entrada corrente do processo, coloca o dado no reg A
static void so_chamada_le(so_t *self)
{
    int term = (processo_pega_id(self->proc_atual) - 1) % 4 * 4;
    int estado;
    if (es_le(self->es, D_TERM_A_TECLADO_OK + term, &estado) != ERR_OK) {
        self->erro_interno = true;
        return;
    }
    if (estado == 0) {
        so_bloqueia_processo(self, PROC_BLOQ_ENTRADA);
        return;
    }
    int dado;
    if (es_le(self->es, D_TERM_A_TECLADO + term, &dado) != ERR_OK) {
        self->erro_interno = true;
        return;
    }
    processo_salva_reg_a(self->proc_atual, dado);
}

// implementação da chamada se sistema SO_ESCR
// escreve o valor do reg X na saída corrente do processo
static void so_chamada_escr(so_t *self)
{
    int term = (processo_pega_id(self->proc_atual) - 1) % 4 * 4;
    int estado;
    if (es_le(self->es, D_TERM_A_TELA_OK + term, &estado) != ERR_OK) {
        self->erro_interno = true;
        return;
    }
    if (estado == 0) {
        so_bloqueia_processo(self, PROC_BLOQ_SAIDA);
        return;
    }
    int dado = processo_pega_reg_x(self->proc_atual);
    if (es_escreve(self->es, D_TERM_A_TELA + term, dado) != ERR_OK) {
        self->erro_interno = true;
        return;
    }
    processo_salva_reg_a(self->proc_atual, 0);
}

// implementação da chamada se sistema SO_CRIA_PROC
// cria um processo
static void so_chamada_cria_proc(so_t *self)
{
  // ainda sem suporte a processos, carrega programa e passa a executar ele
  // quem chamou o sistema não vai mais ser executado, coitado!
  // T1: deveria criar um novo processo

  // em X está o endereço onde está o nome do arquivo
  int ender_proc;
  // t1: deveria ler o X do descritor do processo criador
  if (mem_le(self->mem, IRQ_END_X, &ender_proc) == ERR_OK) {
    char nome[100];
    if (copia_str_da_mem(100, nome, self->mem, ender_proc)) {
      processo_t *p_novo = so_cria_processo(self, nome);
      int ender_carga = processo_pega_reg_pc(p_novo);
      if (ender_carga > 0) {
        // t1: deveria escrever no PC do descritor do processo criado
        processo_salva_reg_a(self->proc_atual, processo_pega_id(p_novo));
        console_printf("Criado processo com PID %d", processo_pega_id(p_novo));
        return;
      } // else?
    }
  }
  // deveria escrever -1 (se erro) ou o PID do processo criado (se OK) no reg A
  //   do processo que pediu a criação
  processo_salva_reg_a(self->proc_atual, -1);
  console_printf("Processo não pôde ser criado");
}

// implementação da chamada se sistema SO_MATA_PROC
// mata o processo com pid X (ou o processo corrente se X é 0)
static void so_chamada_mata_proc(so_t *self)
{
  // T1: deveria matar um processo
  console_printf("Matando processo");
  if (self->proc_atual == NULL) return;
  int id = processo_pega_reg_x(self->proc_atual);
  if (so_mata_processo(self, id) == 0) {
      mem_escreve(self->mem, IRQ_END_A, 0);
      console_printf("Morto processo com PID %d", id);
  }
  else {
      mem_escreve(self->mem, IRQ_END_A, -1);
  }
}

// implementação da chamada se sistema SO_ESPERA_PROC
// espera o fim do processo com pid X
static void so_chamada_espera_proc(so_t *self)
{
  processo_t *p = self->proc_atual;
  int pid = processo_pega_reg_x(p);
  if (pid == processo_pega_id(p)) {
      return;
  }
  for (int i = 0; i < PROC_TAM_TABELA; i++) {
      if (pid == processo_pega_id(self->proc_tabela[i])) {
          so_bloqueia_processo(self, PROC_BLOQ_ESPERA);
          return;
      }
  }
}

// CARGA DE PROGRAMA {{{1

// carrega o programa na memória
// retorna o endereço de carga ou -1
static int so_carrega_programa(so_t *self, char *nome_do_executavel)
{
  // programa para executar na nossa CPU
  programa_t *prog = prog_cria(nome_do_executavel);
  if (prog == NULL) {
    console_printf("Erro na leitura do programa '%s'\n", nome_do_executavel);
    return -1;
  }

  int end_ini = prog_end_carga(prog);
  int end_fim = end_ini + prog_tamanho(prog);

  for (int end = end_ini; end < end_fim; end++) {
    if (mem_escreve(self->mem, end, prog_dado(prog, end)) != ERR_OK) {
      console_printf("Erro na carga da memória, endereco %d\n", end);
      return -1;
    }
  }

  prog_destroi(prog);
  console_printf("SO: carga de '%s' em %d-%d", nome_do_executavel, end_ini, end_fim);
  return end_ini;
}

// ACESSO À MEMÓRIA DOS PROCESSOS {{{1

// copia uma string da memória do simulador para o vetor str.
// retorna false se erro (string maior que vetor, valor não char na memória,
//   erro de acesso à memória)
// T1: deveria verificar se a memória pertence ao processo
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender)
{
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    if (mem_le(mem, ender + indice_str, &caractere) != ERR_OK) {
      return false;
    }
    if (caractere < 0 || caractere > 255) {
      return false;
    }
    str[indice_str] = caractere;
    if (caractere == 0) {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}

// vim: foldmethod=marker
