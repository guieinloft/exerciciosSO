#ifndef PROCESSO_H
#define PROCESSO_H

typedef struct processo_t processo_t;
typedef struct tabela_t tabela_t;

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

int processo_pega_reg_pc(processo_t *self);
int processo_pega_reg_a(processo_t *self);
int processo_pega_reg_x(processo_t *self);
int processo_pega_id(processo_t *self);

void processo_muda_quantum(processo_t *self, int quantum);
void processo_decrementa_quantum(processo_t *self);
int processo_pega_quantum(processo_t *self);
void processo_reseta_quantum(processo_t *self);

void processo_muda_priori(processo_t *self, double priori);
void processo_recalcula_priori(processo_t *self);
double processo_pega_priori(processo_t *self);


// TABELA
tabela_t *tabela_cria(int cap);
int tabela_adiciona_processo(tabela_t *self, processo_t *proc);
void tabela_manda_fim_fila(tabela_t *self);
int tabela_busca_processo(tabela_t *self, int pid);
int tabela_remove_processo(tabela_t *self, int pid);
processo_t *tabela_pega_processo(tabela_t *self, int pid);
processo_t *tabela_pega_processo_indice(tabela_t *self, int indice);
int tabela_pega_tam(tabela_t *self);
void tabela_reordena_priori(tabela_t *self);

#endif
