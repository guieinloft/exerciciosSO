#ifndef PROCESSO_H
#define PROCESSO_H

typedef struct processo_t processo_t;

typedef enum {
    PROC_PRONTO,
    PROC_EXECUTANDO,
    N_PROC_ESTADO
} proc_estado_t;

processo_t *processo_cria(int id, int pc);
void processo_mata(processo_t *self);

void processo_muda_estado(processo_t *self, proc_estado_t e_novo);
proc_estado_t processo_pega_estado(processo_t *self);

void processo_salva_reg_pc(processo_t *self, int pc);
void processo_salva_reg_a(processo_t *self, int a);
void processo_salva_reg_x(processo_t *self, int x);

int processo_pega_reg_pc(processo_t *self);
int processo_pega_reg_a(processo_t *self);
int processo_pega_reg_x(processo_t *self);
int processo_pega_id(processo_t *self);

#endif
