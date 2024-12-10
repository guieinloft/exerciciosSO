#ifndef PROCESSO_H
#define PROCESSO_H

#include "tabpag.h"

typedef struct processo_t processo_t;
typedef struct historico_t historico_t;

typedef enum {
    PROC_PRONTO,
    PROC_EXECUTANDO,
    PROC_BLOQUEADO,
    N_PROC_ESTADO
} proc_estado_t;

typedef enum {
    PROC_BLOQ_ENTRADA,
    PROC_BLOQ_SAIDA,
    PROC_BLOQ_ESPERA,
    N_PROC_BLOQUEIO
} proc_bloqueio_t;

typedef struct proc_metricas_t {
    int n_preempcoes;
    int tempo_retorno;
    int tempo_resposta;

    int estado_vezes[N_PROC_ESTADO];
    int estado_tempo[N_PROC_ESTADO];
} proc_metricas_t;

// PROCESSOS
processo_t *processo_cria(int id, int pc, int max_quantum);
void processo_mata(processo_t *self);

void processo_muda_estado(processo_t *self, proc_estado_t e_novo);
proc_estado_t processo_pega_estado(processo_t *self);

void processo_bloqueia(processo_t *self, proc_bloqueio_t motivo);
proc_bloqueio_t processo_pega_bloq_motivo(processo_t *self);

void processo_salva_reg_pc(processo_t *self, int pc);
void processo_salva_reg_a(processo_t *self, int a);
void processo_salva_reg_x(processo_t *self, int x);
void processo_salva_reg_comp(processo_t *self, int comp);

int processo_pega_reg_pc(processo_t *self);
int processo_pega_reg_a(processo_t *self);
int processo_pega_reg_x(processo_t *self);
int processo_pega_reg_comp(processo_t *self);
int processo_pega_id(processo_t *self);

void processo_muda_quantum(processo_t *self, int quantum);
void processo_decrementa_quantum(processo_t *self);
int processo_pega_quantum(processo_t *self);
void processo_reseta_quantum(processo_t *self);

void processo_muda_priori(processo_t *self, double priori);
void processo_recalcula_priori(processo_t *self);
double processo_pega_priori(processo_t *self);

tabpag_t *processo_pega_tabpag(processo_t *self);

// METRICAS
void processo_atualiza_metricas(processo_t *self, int delta);

// HISTORICO
historico_t *historico_adiciona_metrica(historico_t *hist, processo_t *proc);
int historico_pega_id(historico_t *hist);
proc_metricas_t historico_pega_metricas(historico_t *hist);
void historico_apaga(historico_t *hist);
historico_t *historico_prox(historico_t *hist);

#endif
