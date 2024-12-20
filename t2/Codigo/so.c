// so.c
// sistema operacional
// simulador de computador
// so24b

// INCLUDES {{{1
#include "so.h"
#include "dispositivos.h"
#include "irq.h"
#include "programa.h"
#include "tabpag.h"
#include "instrucao.h"
#include "processo.h"
#include "escalonador.h"
#include "mem_quadro.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

// CONSTANTES E TIPOS {{{1
// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas

// Não tem processos nem memória virtual, mas é preciso usar a paginação,
//   pelo menos para implementar relocação, já que os programas estão sendo
//   todos montados para serem executados no endereço 0 e o endereço 0
//   físico é usado pelo hardware nas interrupções.
// Os programas estão sendo carregados no início de um quadro, e usam quantos
//   quadros forem necessárias. Para isso a variável quadro_livre contém
//   o número do primeiro quadro da memória principal que ainda não foi usado.
//   Na carga do processo, a tabela de páginas (deveria ter uma por processo,
//   mas não tem processo) é alterada para que o endereço virtual 0 resulte
//   no quadro onde o programa foi carregado. Com isso, o programa carregado
//   é acessível, mas o acesso ao anterior é perdido.

// t2: a interface de algumas funções que manipulam memória teve que ser alterada,
//   para incluir o processo ao qual elas se referem. Para isso, precisa de um
//   tipo para o processo. Neste código, não tem processos implementados, e não
//   tem um tipo para isso. Chutei o tipo int. Foi necessário também um valor para
//   representar a inexistência de um processo, coloquei -1. Altere para o seu
//   tipo, ou substitua os usos de processo_t e NENHUM_PROCESSO para o seu tipo.
#define NENHUM_PROCESSO NULL
#define PROC_TAM_TABELA 1000
#define MAX_QUANTUM 5

#define ESC_TIPO ESC_PRIORIDADE
#define MEM_Q_TIPO MEM_Q_SC

#define TEMPO_DISCO 10

typedef struct so_metricas_t {
    int t_exec;
    int t_ocioso;
    int n_preempcoes;
    int n_pag_ausente;
    int n_irq[N_IRQ];
} so_metricas_t;

struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  mem_t *disco;
  mmu_t *mmu;
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

  // primeiro quadro da memória que está livre (quadros anteriores estão ocupados)
  // t2: com memória virtual, o controle de memória livre e ocupada é mais
  //     completo que isso
  mem_quadros_t *quadros;
  int quadro_livre_pri;
  int quadro_livre_sec;

  int t_disco;
};


// função de tratamento de interrupção (entrada no SO)
static int so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
// no t2, foi adicionado o 'processo' aos argumentos dessas funções 
// carrega o programa na memória virtual de um processo; retorna end. inicial
static int so_carrega_programa(so_t *self, processo_t *processo,
                               char *nome_do_executavel);
// copia para str da memória do processo, até copiar um 0 (retorna true) ou tam bytes
static bool so_copia_str_do_processo(so_t *self, int tam, char str[tam],
                                     int end_virt, processo_t *processo);

// CRIAÇÃO {{{1


so_t *so_cria(cpu_t *cpu, mem_t *mem, mem_t *disco, mmu_t *mmu,
              es_t *es, console_t *console)
{
  so_t *self = malloc(sizeof(*self));
  assert(self != NULL);

  self->cpu = cpu;
  self->mem = mem;
  self->disco = disco;
  self->mmu = mmu;
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
  self->metricas.n_pag_ausente = 0;

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
  int ender = so_carrega_programa(self, NENHUM_PROCESSO, "trata_int.maq");
  if (ender != IRQ_END_TRATADOR) {
    console_printf("SO: problema na carga do programa de tratamento de interrupção");
    self->erro_interno = true;
  }

  // programa o relógio para gerar uma interrupção após INTERVALO_INTERRUPCAO
  if (es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO) != ERR_OK) {
    console_printf("SO: problema na programação do timer");
    self->erro_interno = true;
  }

  // define o primeiro quadro livre de memória como o seguinte àquele que
  //   contém o endereço 99 (as 100 primeiras posições de memória (pelo menos)
  //   não vão ser usadas por programas de usuário)
  // t2: o controle de memória livre deve ser mais aprimorado que isso  
  self->quadro_livre_pri = 99 / TAM_PAGINA + 1;
  self->quadro_livre_sec = 0;

  self->quadros = mem_quadros_cria(MEM_TAM / TAM_PAGINA, 99 / TAM_PAGINA + 1, MEM_Q_TIPO);

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
static void so_adiciona_processo(so_t *self, processo_t *p) {
    if (p == NULL) return;
    for (int i = 0; i < PROC_TAM_TABELA; i++) {
        if (self->proc_tabela[i] == NULL) {
            self->proc_tabela[i] = p;
            break;
        }
    }
    escalonador_adiciona_processo(self->esc, p);
    self->proc_ultimo_id++;
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
    mem_quadros_remove_processo(self->quadros, processo_pega_id(self->proc_tabela[index]));

    processo_mata(self->proc_tabela[index]);
    self->proc_tabela[index] = NULL;
    return 0;
}

static void so_bloqueia_processo(so_t *self, proc_bloqueio_t motivo) {
    if (motivo == PROC_BLOQ_DISCO) {
        processo_bloqueia_disco(self->proc_atual, self->t_disco);
    }
    else {
        processo_bloqueia(self->proc_atual, motivo);
    }
    escalonador_remove_processo(self->esc, 0);
    console_printf("Processo %d bloqueado", processo_pega_id(self->proc_atual));
}

static void so_desbloqueia_processo(so_t *self, processo_t *proc) {
    processo_muda_estado(proc, PROC_PRONTO);
    escalonador_adiciona_processo(self->esc, proc);
    console_printf("Processo %d desbloqueado", processo_pega_id(proc));
}

static processo_t *so_busca_processo(so_t *self, int pid) {
    for (int i = 0; i < PROC_TAM_TABELA; i++) {
        if (self->proc_tabela[i] != NULL && processo_pega_id(self->proc_tabela[i]) == pid) {
            return self->proc_tabela[i];
        }
    }
    return NULL;
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
    /*
    console_printf("Tempo ocioso:   %d", self->metricas.t_ocioso);
    console_printf("N. preempções:  %d", self->metricas.n_preempcoes);
    console_printf("N. pag. ausente:  %d", self->metricas.n_pag_ausente);

    for (int i = 0; i < N_IRQ; i++) {
        console_printf("N. interrup. %d: %d", i, self->metricas.n_irq[i]);
    }
    */
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
    sprintf(nome, "../Metricas/log_%d_%d.txt", MEM_TAM, TAM_PAGINA);
    FILE *log = fopen(nome, "w");
    fprintf(log, "Tempo execução: %d\n", self->metricas.t_exec);
    fprintf(log, "Tempo ocioso:   %d\n", self->metricas.t_ocioso);
    fprintf(log, "N. preempções:  %d\n", self->metricas.n_preempcoes);
    fprintf(log, "N. pag_ausente:  %d\n", self->metricas.n_pag_ausente);

    for (int i = 0; i < N_IRQ; i++) {
        fprintf(log, "N. interrup. %d: %d\n", i, self->metricas.n_irq[i]);
    }
    for (historico_t *h = self->hist; h != NULL; h = historico_prox(h)) {
        proc_metricas_t metricas = historico_pega_metricas(h);
        fprintf(log, "\nID: %d\n", historico_pega_id(h));
        fprintf(log, "N. preempções: %d\n", metricas.n_preempcoes);
        fprintf(log, "T. retorno:    %d\n", metricas.tempo_retorno);
        fprintf(log, "T. resposta:   %d\n", metricas.tempo_resposta);
        fprintf(log, "N. pag. ausente:   %d\n", metricas.n_pag_ausente);
        for (int i = 0; i < N_PROC_ESTADO; i++) {
            fprintf(log, "Vezes em %d: %d\n", i, metricas.estado_vezes[i]);
            fprintf(log, "Tempo em %d: %d\n", i, metricas.estado_tempo[i]);
        }
        for (int i = 0; i < N_PROC_BLOQUEIO; i++) {
            fprintf(log, "Vezes bloqueado por %d: %d\n", i, metricas.bloqueio_vezes[i]);
            fprintf(log, "Tempo bloqueado por %d: %d\n", i, metricas.bloqueio_tempo[i]);
        }
    }
}

// TRATAMENTO DE BLOQUEIO {{{1
static int so_trata_bloq_espera(so_t *self, processo_t *p) {
  int pid = processo_pega_reg_x(p);
  console_printf("%d", pid);
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

static int so_trata_bloq_disco(so_t *self, processo_t *p) {
  processo_decrementa_t_disco(p);
  self->t_disco--;
  console_printf("TEMPO: %d", processo_pega_t_disco(p));
  if (processo_pega_t_disco(p) <= 0) {
      console_printf("DESBLOQUEEI PROCESSO DISCO");
      so_desbloqueia_processo(self, p);
      return 1;
  }
  return 0;
}

// TRATAMENTO DE PAGE FAULT {{{1
static int so_move_disco_mem(so_t *self, int end_disco_ini, int end_mem_ini) {
    int dado;
    for (int i = 0; i < TAM_PAGINA; i++) {
        if (mem_le(self->disco, end_disco_ini + i, &dado) != ERR_OK) {
            console_printf("Erro na leitura do disco");
            return -1;
        }
        if (mem_escreve(self->mem, end_mem_ini + i, dado) != ERR_OK) {
            console_printf("Erro na escrita na memória");
            return -1;
        }
        console_printf("Lendo de D-%d e escrevendo em M-%d", end_disco_ini + i, end_mem_ini + i);
    }
    return 0;
}

static int so_move_mem_disco(so_t *self, int end_mem_ini, int end_disco_ini) {
  int dado;
    for (int i = 0; i < TAM_PAGINA; i++) {
      if (mem_le(self->mem, end_mem_ini + i, &dado) != ERR_OK) {
        console_printf("Erro na leitura da memória");
        return -1;
      }
      if (mem_escreve(self->disco, end_disco_ini + i, dado) != ERR_OK) {
        console_printf("Erro na escrita no disco");
        return -1;
      }
    }
  return 0;
}

static int so_escolhe_pagina_pra_liberar(so_t *self) {
    while(1) {
        int pid = mem_quadros_pega_dono(self->quadros, -1);
        int pagina = mem_quadros_pega_pagina(self->quadros, -1);
        processo_t *p = so_busca_processo(self, pid);
        tabpag_t *tab = processo_pega_tabpag(p);
        bool r = tabpag_bit_acesso(tab, pagina);
        if (!r) {
            return mem_quadros_libera_quadro_fifo(self->quadros);
        }
        mem_quadros_manda_fim_fila(self->quadros);
        tabpag_zera_bit_acesso(tab, pagina);
    }
}

static int so_libera_pagina(so_t *self) {
    console_printf("SUBSTITUINDO PAGINA");
    int quadro_livre; 
    switch(MEM_Q_TIPO) {
        case MEM_Q_SC:
        quadro_livre = so_escolhe_pagina_pra_liberar(self);
        break;
        default: 
        quadro_livre = mem_quadros_libera_quadro_fifo(self->quadros);
        break;
    }
    int pid = mem_quadros_pega_dono(self->quadros, quadro_livre);
    int pagina = mem_quadros_pega_pagina(self->quadros, quadro_livre);
    processo_t *p = so_busca_processo(self, pid);
    tabpag_t *tab = processo_pega_tabpag(p);
    console_printf("BIT ALT: %d", tabpag_bit_alteracao(tab, pagina));
    if (tabpag_bit_alteracao(tab, pagina)) {
        int pag_disco = processo_pega_pagina_disco(p) + pagina;
        int end_disco_ini = pag_disco * TAM_PAGINA;
        int end_mem_ini = quadro_livre * TAM_PAGINA;
        so_move_mem_disco(self, end_mem_ini, end_disco_ini);
    }
    tabpag_invalida_pagina(tab, pagina);
    console_printf("Liberado pg %d do proc %d", pagina, pid);
    return quadro_livre;
}

static void so_trata_pag_ausente(so_t *self, int comp) {
    console_printf("COMP: %d", comp);
    int pag_disco = processo_pega_pagina_disco(self->proc_atual) + comp/TAM_PAGINA;
    int end_disco_ini = pag_disco * TAM_PAGINA;
    console_printf("END_DISCO = %d", end_disco_ini);
    int quadro_livre = mem_quadros_tem_livre(self->quadros);
    if (quadro_livre == -1) {
        quadro_livre = so_libera_pagina(self);
    }
    console_printf("QUADRO LIVRE: %d", quadro_livre);
    int end_mem_ini = quadro_livre * TAM_PAGINA;
    so_move_disco_mem(self, end_disco_ini, end_mem_ini);
    tabpag_define_quadro(processo_pega_tabpag(self->proc_atual), comp / TAM_PAGINA, quadro_livre);
    mem_quadros_muda_estado(self->quadros, quadro_livre, 0, processo_pega_id(self->proc_atual), comp / TAM_PAGINA);
    console_printf("Página %d nos endereços %d-%d", comp/TAM_PAGINA, quadro_livre * TAM_PAGINA, (quadro_livre + 1) * TAM_PAGINA - 1);
    processo_atualiza_n_pag_ausente(self->proc_atual);
    self->metricas.n_pag_ausente++;
    self->t_disco += TEMPO_DISCO;
    so_bloqueia_processo(self, PROC_BLOQ_DISCO);
    console_printf("BLOQUEIO DISCO");
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
      int retorno = so_despacha(self);
      console_printf("RETORNO DESPACHA: %d", retorno);
      return retorno;
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
  int pc, a, x, comp;
  mem_le(self->mem, IRQ_END_PC, &pc);
  mem_le(self->mem, IRQ_END_A, &a);
  mem_le(self->mem, IRQ_END_X, &x);
  mem_le(self->mem, IRQ_END_complemento, &comp);
  processo_salva_reg_pc(self->proc_atual, pc);
  processo_salva_reg_a(self->proc_atual, a);
  processo_salva_reg_x(self->proc_atual, x);
  processo_salva_reg_comp(self->proc_atual, comp);
}

static void so_trata_pendencias(so_t *self)
{
    for (int i = 0; i < PROC_TAM_TABELA; i++) {
        processo_t *p = self->proc_tabela[i];
        if (p != NULL) console_printf("pend %d %d", i, processo_pega_estado(p));
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
                case PROC_BLOQ_DISCO:
                so_trata_bloq_disco(self, p);
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
  // passa o processador para modo usuário
  mem_escreve(self->mem, IRQ_END_erro, ERR_OK);
  if (self->erro_interno) return 1;
  if (self->proc_atual == NULL) return 1;
  if (processo_pega_estado(self->proc_atual) != PROC_EXECUTANDO) return 1;
  int pc, a, x, comp;
  pc = processo_pega_reg_pc(self->proc_atual);
  a = processo_pega_reg_a(self->proc_atual);
  x = processo_pega_reg_x(self->proc_atual);
  comp = processo_pega_reg_comp(self->proc_atual);
  mem_escreve(self->mem, IRQ_END_PC, pc);
  mem_escreve(self->mem, IRQ_END_A, a);
  mem_escreve(self->mem, IRQ_END_X, x);
  mem_escreve(self->mem, IRQ_END_complemento, comp);
  mmu_define_tabpag(self->mmu, processo_pega_tabpag(self->proc_atual));
  mem_escreve(self->mem, IRQ_END_erro, ERR_OK);
  int inst;
  mmu_le(self->mmu, pc+1, &inst, usuario);
  console_printf("PC: %d, INST: %d", pc, inst);
  console_printf("Página: %d", pc/TAM_PAGINA);
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

  // coloca o programa "init" na memória
  // t2: deveria criar um processo, e programar a tabela de páginas dele
  processo_t *processo = processo_cria(self->proc_ultimo_id + 1, 0, MAX_QUANTUM); // deveria inicializar um processo...
  int ender = so_carrega_programa(self, processo, "init.maq");
  if (ender != 0) {
    console_printf("SO: problema na carga do programa inicial");
    self->erro_interno = true;
    return;
  }
  so_adiciona_processo(self, processo);
  self->proc_atual = processo;

  // altera o PC para o endereço de carga (deve ter sido o endereço virtual 0)
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
  if(err == ERR_PAG_AUSENTE) {
      int comp;
      comp = processo_pega_reg_comp(self->proc_atual);
      so_trata_pag_ausente(self, comp);
      return;
  }
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
  processo_t *processo = processo_cria(self->proc_ultimo_id + 1, 0, MAX_QUANTUM); // T2: o processo criado

  // em X está o endereço onde está o nome do arquivo
  int ender_proc;
  // t1: deveria ler o X do descritor do processo criador
  if (mem_le(self->mem, IRQ_END_X, &ender_proc) == ERR_OK) {
    char nome[100];
    processo_t *p2 = processo; // com TAM_PAGINA < 7, o processo principal corrompe, logo isso aqui é um workaround
    bool copia_str = so_copia_str_do_processo(self, 100, nome, ender_proc, processo);
    console_printf("%llu", p2);
    if (copia_str) {
      console_printf("Chegou até aqui");
      int ender_carga = so_carrega_programa(self, processo, nome);
      // o endereço de carga é endereço virtual, deve ser 0
      if (ender_carga == 0) {
        // deveria escrever no PC do descritor do processo criado
        so_adiciona_processo(self, processo);
        processo_salva_reg_a(self->proc_atual, self->proc_ultimo_id);
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
  if (pid <= processo_pega_id(p)) {
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

// funções auxiliares
static int so_carrega_programa_na_memoria_fisica(so_t *self, programa_t *programa);
static int so_carrega_programa_na_memoria_virtual(so_t *self,
                                                  programa_t *programa,
                                                  processo_t *processo);

// carrega o programa na memória de um processo ou na memória física se NENHUM_PROCESSO
// retorna o endereço de carga ou -1
static int so_carrega_programa(so_t *self, processo_t *processo,
                               char *nome_do_executavel)
{
  console_printf("SO: carga de '%s'", nome_do_executavel);

  programa_t *programa = prog_cria(nome_do_executavel);
  if (programa == NULL) {
    console_printf("Erro na leitura do programa '%s'\n", nome_do_executavel);
    return -1;
  }

  int end_carga;
  if (processo == NENHUM_PROCESSO) {
    end_carga = so_carrega_programa_na_memoria_fisica(self, programa);
  } else {
    end_carga = so_carrega_programa_na_memoria_virtual(self, programa, processo);
  }

  prog_destroi(programa);
  return end_carga;
}

static int so_carrega_programa_na_memoria_fisica(so_t *self, programa_t *programa)
{
  int end_ini = prog_end_carga(programa);
  int end_fim = end_ini + prog_tamanho(programa);

  for (int end = end_ini; end < end_fim; end++) {
    if (mem_escreve(self->mem, end, prog_dado(programa, end)) != ERR_OK) {
      console_printf("Erro na carga da memória, endereco %d\n", end);
      return -1;
    }
  }
  console_printf("carregado na memória física, %d-%d", end_ini, end_fim);
  return end_ini;
}

static int so_carrega_programa_na_memoria_virtual(so_t *self,
                                                  programa_t *programa,
                                                  processo_t *processo)
{
  // t2: isto tá furado...
  // está simplesmente lendo para o próximo quadro que nunca foi ocupado,
  //   nem testa se tem memória disponível
  // com memória virtual, a forma mais simples de implementar a carga de um
  //   programa é carregá-lo para a memória secundária, e mapear todas as páginas
  //   da tabela de páginas do processo como inválidas. Assim, as páginas serão
  //   colocadas na memória principal por demanda. Para simplificar ainda mais, a
  //   memória secundária pode ser alocada da forma como a principal está sendo
  //   alocada aqui (sem reuso)
  int end_virt_ini = prog_end_carga(programa);
  int end_virt_fim = end_virt_ini + prog_tamanho(programa) - 1;
  int pagina_ini = end_virt_ini / TAM_PAGINA;
  int pagina_fim = end_virt_fim / TAM_PAGINA;
  int quadro_ini = self->quadro_livre_sec;
  processo_salva_pagina_disco(processo, quadro_ini);
  // mapeia as páginas nos quadros
  int quadro = quadro_ini;
  for (int pagina = pagina_ini; pagina <= pagina_fim; pagina++) {
    //tabpag_define_quadro(processo_pega_tabpag(processo), pagina, quadro);
    quadro++;
  }
  self->quadro_livre_sec = quadro;

  // carrega o programa na memória principal
  int end_fis_ini = quadro_ini * TAM_PAGINA;
  int end_fis = end_fis_ini;
  for (int end_virt = end_virt_ini; end_virt <= end_virt_fim; end_virt++) {
    if (mem_escreve(self->disco, end_fis, prog_dado(programa, end_virt)) != ERR_OK) {
      console_printf("Erro na carga da memória, end virt %d fís %d\n", end_virt,
                     end_fis);
      return -1;
    }
    end_fis++;
  }
  console_printf("carregado na memória virtual V%d-%d F%d-%d",
                 end_virt_ini, end_virt_fim, end_fis_ini, end_fis - 1);
  return end_virt_ini;
}

// ACESSO À MEMÓRIA DOS PROCESSOS {{{1

// copia uma string da memória do processo para o vetor str.
// retorna false se erro (string maior que vetor, valor não char na memória,
//   erro de acesso à memória)
// O endereço é um endereço virtual de um processo.
// T2: Com memória virtual, cada valor do espaço de endereçamento do processo
//   pode estar em memória principal ou secundária (e tem que achar onde)
static bool so_copia_str_do_processo(so_t *self, int tam, char str[tam],
                                     int end_virt, processo_t *processo)
{
  if (processo == NENHUM_PROCESSO) return false;
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    // não tem memória virtual implementada, posso usar a mmu para traduzir
    //   os endereços e acessar a memória
    err_t erro_mmu = mmu_le(self->mmu, end_virt + indice_str, &caractere, usuario);
    if (erro_mmu != ERR_OK) {
        if (erro_mmu == ERR_PAG_AUSENTE) {
            console_printf("ERRO: PAGINA AUSENTE");
            so_trata_pag_ausente(self, end_virt + indice_str);
            indice_str--;
        }
        else {
          console_printf("ERRO: %d", erro_mmu);
          return false;
        }
    }
    else if (caractere < 0 || caractere > 255) {
      console_printf("ERRO: CARACTER INVALIDO");
      return false;
    }
    
    str[indice_str] = caractere;
    if (caractere == 0) {
      return true;
    }
  }
  console_printf("ERRO: ESTOUROU TAMANHO");
  return false;
}

// vim: foldmethod=marker
