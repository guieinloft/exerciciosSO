#ifndef PROCESSO_H
#define PROCESSO_H

typedef struct processo_t processo_t;

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

processo_t *processo_cria(int id, int pc);
void processo_mata(processo_t *self);

void processo_muda_estado(processo_t *self, proc_estado_t e_novo);
proc_estado_t processo_pega_estado(processo_t *self);

void processo_bloqueia(processo_t *self, proc_bloqueio_t motivo);
proc_bloqueio_t processo_pega_bloq_motivo(processo_t *self);

void processo_salva_reg_pc(processo_t *self, int pc);
void processo_salva_reg_a(processo_t *self, int a);
void processo_salva_reg_x(processo_t *self, int x);

int processo_pega_reg_pc(processo_t *self);
int processo_pega_reg_a(processo_t *self);
int processo_pega_reg_x(processo_t *self);
int processo_pega_id(processo_t *self);

void processo_muda_quantum(processo_t *self, int quantum);
void processo_decrementa_quantum(processo_t *self);
int processo_pega_quantum(processo_t *self);

#endif
