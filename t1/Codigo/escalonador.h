#ifndef ESCALONADOR_H
#define ESCALONADOR_H

typedef struct esc_t esc_t;

typedef enum {
    ESC_SIMPLES,
    ESC_CIRCULAR,
    ESC_PRIORIDADE
} esc_tipo_t;

esc_t *escalonador_cria(tabela_t *t);

void escalonador_cria_proc(esc_t *self, processo_t *proc);
void escalonador_mata_proc(esc_t *self, processo_t *proc);
int escalonador_pega_atual(esc_t *self);

int escalonador_simples(esc_t *self, so_t *so);
int escalonador_circular(esc_t *self, so_t *so);
int escalonador_prioridade(esc_t *self, so_t *so);

#endif
