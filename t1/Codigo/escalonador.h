#ifndef ESCALONADOR_H
#define ESCALONADOR_H

typedef struct esc_t esc_t;

typedef enum {
    ESC_SIMPLES,
    ESC_CIRCULAR,
    ESC_PRIORIDADE
} esc_tipo_t;

esc_t *escalonador_cria(int cap);
void escalonador_destroi(esc_t *self);

int escalonador_adiciona_processo(esc_t *self, processo_t *proc);
void escalonador_remove_processo(esc_t *self, int pid);
processo_t *escalonador_pega_atual(esc_t *self);

int escalonador_simples(esc_t *self, so_t *so);
int escalonador_circular(esc_t *self, so_t *so);
int escalonador_prioridade(esc_t *self, so_t *so);

#endif
