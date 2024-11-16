#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "so.h"
#include "processo.h"
#include "escalonador.h"

#define MAX_QUANTUM 5

typedef struct e_no_t {
    processo_t *proc;
    struct e_no_t *prox;
    struct e_no_t *ant;
} e_no_t;

struct esc_t {
    int max_quantum;
    e_no_t *ini;
};

esc_t *escalonador_cria() {
    esc_t *esc = (esc_t*)malloc(sizeof(esc_t));
    esc->max_quantum = MAX_QUANTUM;
    esc->ini = NULL;
    return esc;
}

void escalonador_cria_proc(esc_t *self, processo_t *proc){
    e_no_t *n = (e_no_t*)malloc(sizeof(e_no_t));
    assert(n != NULL);
    n->proc = proc;
    processo_muda_quantum(n->proc, self->max_quantum);
    if (self->ini == NULL) {
        self->ini = n;
        n->ant = n;
        n->prox = n;
    }
    else {
        n->ant = self->ini->ant;
        n->ant->prox = n;
        n->prox = self->ini;
        self->ini->ant = n;
    }
}

void escalonador_mata_proc(esc_t *self, processo_t *proc){
    e_no_t *ini = self->ini;
    e_no_t *n = ini;
    do {
        if (n->proc == proc) {
            if (n->ant == n) {
                self->ini = NULL;
            }
            else {
                n->ant->prox = n->prox;
                n->prox->ant = n->ant;
                if (self->ini == n) {
                    self->ini = n->prox;
                }
            }
            free(n);
            return;
        }
        n = n->prox;
    } while (n != ini);
}

int escalonador_pega_atual(esc_t *self) {
    if (self->ini != NULL) return processo_pega_id(self->ini->proc);
    return 0;
}

void escalonador_simples(esc_t *self, so_t *so) {
    e_no_t *ini = self->ini;
    e_no_t *n = ini;
    do {
        processo_t *p = n->proc; 
        if (p != NULL) {
            if (processo_pega_estado(p) == PROC_EXECUTANDO) {
                break;
            }
            if (processo_pega_estado(p) == PROC_PRONTO) {
                processo_muda_estado(p, PROC_EXECUTANDO);
                break;
            }
            self->ini = n->prox;
            n = n->prox;
        }
    } while (n != ini);
}

void escalonador_circular(esc_t *self, so_t *so) {
    e_no_t *ini = self->ini;
    e_no_t *n = ini;
    do {
        processo_t *p = n->proc; 
        if (p != NULL) {
            if (processo_pega_estado(p) == PROC_EXECUTANDO) {
                if (processo_pega_quantum(p) == 0) {
                    processo_muda_estado(p, PROC_PRONTO);
                    processo_muda_quantum(p, self->max_quantum);
                }
                else return;
            }
            else if (processo_pega_estado(p) == PROC_PRONTO) {
                processo_muda_estado(p, PROC_EXECUTANDO);
                return;
            }
        }
        self->ini = n->prox;
        n = n->prox;
    } while (n != ini);
    if (processo_pega_estado(self->ini->proc) == PROC_PRONTO) {
        processo_muda_estado(self->ini->proc, PROC_EXECUTANDO);
    }
}

void escalonador_prioridade(esc_t *self, so_t *so) {
    // TODO: implementar
    escalonador_circular(self, so);
}
