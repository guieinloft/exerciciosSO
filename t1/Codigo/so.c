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

// CONSTANTES E TIPOS {{{1
// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas

#define PROC_TAM_TABELA 1000

struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  es_t *es;
  console_t *console;
  bool erro_interno;
  // t1: tabela de processos, processo corrente, pendências, etc
  processo_t **proc_tabela;
  int proc_atual;
  int proc_ultimo_id;
  esc_t *esc;
  esc_tipo_t esc_tipo;
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
  self->proc_tabela = (processo_t**)malloc(sizeof(processo_t*)*PROC_TAM_TABELA);
  assert(self->proc_tabela != NULL);
  self->proc_atual = 0;
  self->proc_ultimo_id = 0;

  // cria escalonador
  self->esc = escalonador_cria();
  self->esc_tipo = ESC_CIRCULAR;

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
  free(self);
}

int so_cria_processo(so_t *self, char *origem) {
    console_printf("Criando processo");
    int ender = so_carrega_programa(self, origem);
    processo_t *p = processo_cria(self->proc_ultimo_id + 1, ender);
    escalonador_cria_proc(self->esc, p);
    self->proc_ultimo_id++;
    int i = self->proc_atual;
    while (i < self->proc_atual + PROC_TAM_TABELA) {
        if (self->proc_tabela[i % PROC_TAM_TABELA] == NULL) {
            self->proc_tabela[i % PROC_TAM_TABELA] = p;
            return i % PROC_TAM_TABELA;
        }
        i++;
    }
    return -1;
}

int so_busca_processo(so_t *self, int id) {
    for (int i = 0; i < PROC_TAM_TABELA; i++) {
        if (self->proc_tabela[i] != NULL && processo_pega_id(self->proc_tabela[i]) == id) {
            return i;
        }
    }
    // se chegou até aqui, processo não está na tabela
    return -1;
}

processo_t *so_pega_processo(so_t *self, int indice){
    return self->proc_tabela[indice];
}

// TRATAMENTO DE BLOQUEIO {{{1

static void so_trata_bloq_espera(so_t *self, int ind) {
  processo_t *p = self->proc_tabela[ind];
  int pid = processo_pega_reg_x(p);
  int indice = so_busca_processo(self, pid);
  if(indice == ind || indice == -1) {
      processo_muda_estado(p, PROC_PRONTO);
      console_printf("Desbloqueando processo %d", ind); 
  }
}  

// TRATAMENTO DE INTERRUPÇÃO {{{1

// funções auxiliares para o tratamento de interrupção
static void so_salva_estado_da_cpu(so_t *self);
static void so_trata_irq(so_t *self, int irq);
static void so_trata_pendencias(so_t *self);
static void so_escalona(so_t *self);
static int so_despacha(so_t *self);

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
  return so_despacha(self);
}

static void so_salva_estado_da_cpu(so_t *self)
{
  // t1: salva os registradores que compõem o estado da cpu no descritor do
  //   processo corrente. os valores dos registradores foram colocados pela
  //   CPU na memória, nos endereços IRQ_END_*
  // se não houver processo corrente, não faz nada
  if (self->proc_tabela[self->proc_atual] == NULL) return;
  int pc, a, x;
  mem_le(self->mem, IRQ_END_PC, &pc);
  mem_le(self->mem, IRQ_END_A, &a);
  mem_le(self->mem, IRQ_END_X, &x);
  processo_salva_reg_pc(self->proc_tabela[self->proc_atual], pc);
  processo_salva_reg_a(self->proc_tabela[self->proc_atual], a);
  processo_salva_reg_x(self->proc_tabela[self->proc_atual], x);
}

static void so_trata_pendencias(so_t *self)
{
    for (int i = 0; i < PROC_TAM_TABELA; i++) {
        if (self->proc_tabela[i] != NULL && processo_pega_estado(self->proc_tabela[i]) == PROC_BLOQUEADO) {
            switch (processo_pega_bloq_motivo(self->proc_tabela[i])) {
                case PROC_BLOQ_ESPERA:
                so_trata_bloq_espera(self, i);
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
    console_printf("Escalonando agora", self->proc_atual);
    switch (self->esc_tipo) {
        case ESC_SIMPLES:
        escalonador_simples(self->esc, self);
        break;
        case ESC_CIRCULAR:
        escalonador_circular(self->esc, self);
        break;
        case ESC_PRIORIDADE:
        escalonador_prioridade(self->esc, self);
        break;
    }
    int atual_temp = so_busca_processo(self, escalonador_pega_atual(self->esc));
    self->proc_atual = atual_temp * (atual_temp != -1);
}

static int so_despacha(so_t *self)
{
  // t1: se houver processo corrente, coloca o estado desse processo onde ele
  //   será recuperado pela CPU (em IRQ_END_*) e retorna 0, senão retorna 1
  // o valor retornado será o valor de retorno de CHAMAC
  if (self->erro_interno || self->proc_tabela[self->proc_atual] == NULL) return 1;
  int pc, a, x;
  pc = processo_pega_reg_pc(self->proc_tabela[self->proc_atual]);
  a = processo_pega_reg_a(self->proc_tabela[self->proc_atual]);
  x = processo_pega_reg_x(self->proc_tabela[self->proc_atual]);
  mem_escreve(self->mem, IRQ_END_PC, pc);
  mem_escreve(self->mem, IRQ_END_A, a);
  mem_escreve(self->mem, IRQ_END_X, x);
  return 0;
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
  int ender = so_cria_processo(self, "init.maq");
  if (ender == -1 || processo_pega_reg_pc(self->proc_tabela[ender]) != 100) {
    console_printf("SO: problema na carga do programa inicial");
    self->erro_interno = true;
    return;
  }

  // altera o PC para o endereço de carga
  mem_escreve(self->mem, IRQ_END_PC, ender);
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
  console_printf("SO: IRQ não tratada -- erro na CPU: %s", err_nome(err));
  self->erro_interno = true;
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
  processo_decrementa_quantum(self->proc_tabela[self->proc_atual]);
  console_printf("Quantum %d: %d", self->proc_atual, processo_pega_quantum(self->proc_tabela[self->proc_atual]));
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
  if (self->proc_tabela[self->proc_atual] == NULL) {
    console_printf("SO: erro no acesso ao id da chamada de sistema");
    self->erro_interno = true;
    return;
  }
  id_chamada = processo_pega_reg_a(self->proc_tabela[self->proc_atual]);
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
  // implementação com espera ocupada
  //   T1: deveria realizar a leitura somente se a entrada estiver disponível,
  //     senão, deveria bloquear o processo.
  //   no caso de bloqueio do processo, a leitura (e desbloqueio) deverá
  //     ser feita mais tarde, em tratamentos pendentes em outra interrupção,
  //     ou diretamente em uma interrupção específica do dispositivo, se for
  //     o caso
  // implementação lendo direto do terminal A
  //   T1: deveria usar dispositivo de entrada corrente do processo
  processo_t *p_atual = self->proc_tabela[self->proc_atual];
  if (p_atual == NULL) return;
  int term = (processo_pega_id(p_atual)-1) % 4 * 4;
  for (;;) {
    int estado;
    if (es_le(self->es, D_TERM_A_TECLADO_OK + term, &estado) != ERR_OK) {
      console_printf("SO: problema no acesso ao estado do teclado");
      self->erro_interno = true;
      return;
    }
    if (estado != 0) break;
    // como não está saindo do SO, a unidade de controle não está executando seu laço.
    // esta gambiarra faz pelo menos a console ser atualizada
    // T1: com a implementação de bloqueio de processo, esta gambiarra não
    //   deve mais existir.
    console_tictac(self->console);
  }
  int dado;
  if (es_le(self->es, D_TERM_A_TECLADO + term, &dado) != ERR_OK) {
    console_printf("SO: problema no acesso ao teclado");
    self->erro_interno = true;
    return;
  }
  // escreve no reg A do processador
  // (na verdade, na posição onde o processador vai pegar o A quando retornar da int)
  // T1: se houvesse processo, deveria escrever no reg A do processo
  // T1: o acesso só deve ser feito nesse momento se for possível; se não, o processo
  //   é bloqueado, e o acesso só deve ser feito mais tarde (e o processo desbloqueado)
  processo_salva_reg_a(p_atual, dado);
}

// implementação da chamada se sistema SO_ESCR
// escreve o valor do reg X na saída corrente do processo
static void so_chamada_escr(so_t *self)
{
  // implementação com espera ocupada
  //   T1: deveria bloquear o processo se dispositivo ocupado
  // implementação escrevendo direto do terminal A
  //   T1: deveria usar o dispositivo de saída corrente do processo
  processo_t *p_atual = self->proc_tabela[self->proc_atual];
  if (p_atual == NULL) return;
  int term = (processo_pega_id(p_atual)-1) % 4 * 4;
  for (;;) {
    int estado;
    if (es_le(self->es, D_TERM_A_TELA_OK + term, &estado) != ERR_OK) {
      console_printf("SO: problema no acesso ao estado da tela");
      self->erro_interno = true;
      return;
    }
    if (estado != 0) break;
    // como não está saindo do SO, a unidade de controle não está executando seu laço.
    // esta gambiarra faz pelo menos a console ser atualizada
    // T1: não deve mais existir quando houver suporte a processos, porque o SO não poderá
    //   executar por muito tempo, permitindo a execução do laço da unidade de controle
    console_tictac(self->console);
  }
  int dado;
  // está lendo o valor de X e escrevendo o de A direto onde o processador colocou/vai pegar
  // T1: deveria usar os registradores do processo que está realizando a E/S
  // T1: caso o processo tenha sido bloqueado, esse acesso deve ser realizado em outra execução
  //   do SO, quando ele verificar que esse acesso já pode ser feito.
  dado = processo_pega_reg_x(p_atual);
  if (es_escreve(self->es, D_TERM_A_TELA + term, dado) != ERR_OK) {
    console_printf("SO: problema no acesso à tela");
    self->erro_interno = true;
    return;
  }
  processo_salva_reg_a(p_atual, 0);
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
      int indice = so_cria_processo(self, nome);
      int ender_carga = processo_pega_reg_pc(self->proc_tabela[indice]);
      if (ender_carga > 0) {
        // t1: deveria escrever no PC do descritor do processo criado
        processo_salva_reg_a(self->proc_tabela[self->proc_atual], processo_pega_id(self->proc_tabela[indice]));
        console_printf("Criado processo com PID %d", processo_pega_id(self->proc_tabela[indice]));
        return;
      } // else?
    }
  }
  // deveria escrever -1 (se erro) ou o PID do processo criado (se OK) no reg A
  //   do processo que pediu a criação
  processo_salva_reg_a(self->proc_tabela[self->proc_atual], -1);
  console_printf("Processo não pôde ser criado");
}

// implementação da chamada se sistema SO_MATA_PROC
// mata o processo com pid X (ou o processo corrente se X é 0)
static void so_chamada_mata_proc(so_t *self)
{
  // T1: deveria matar um processo
  console_printf("Matando processo");
  if (self->proc_tabela[self->proc_atual] == NULL) return;
  int id = processo_pega_reg_x(self->proc_tabela[self->proc_atual]);
  int indice;
  if (id == 0) {
      id = processo_pega_id(self->proc_tabela[self->proc_atual]);
      indice = self->proc_atual;
  }
  else {
      indice = so_busca_processo(self, id);
  }
  if (indice != -1) {
      escalonador_mata_proc(self->esc, self->proc_tabela[indice]);
      processo_mata(self->proc_tabela[indice]);
      self->proc_tabela[indice] = NULL;
      mem_escreve(self->mem, IRQ_END_A, 0);
  }
  else {
      mem_escreve(self->mem, IRQ_END_A, -1);
  }
  console_printf("Morto processo com PID %d", id);
}

// implementação da chamada se sistema SO_ESPERA_PROC
// espera o fim do processo com pid X
static void so_chamada_espera_proc(so_t *self)
{
  processo_t *p = self->proc_tabela[self->proc_atual];
  int pid = processo_pega_reg_x(p);
  int indice = so_busca_processo(self, pid);
  if(indice == self->proc_atual || indice == -1) return;
  processo_bloqueia(p, PROC_BLOQ_ESPERA);
  console_printf("Processo com pid %d bloqueado", pid);
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
