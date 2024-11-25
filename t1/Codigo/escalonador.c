#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "so.h"
#include "processo.h"
#include "escalonador.h"
#include "console.h"

#define MAX_QUANTUM 5

struct esc_t {
    int ini;
    int tam;
    int cap;
    processo_t **tab;
    int max_quantum;
};

// CRIAÇÃO
esc_t *escalonador_cria(int cap) {
    esc_t *esc = (esc_t*)malloc(sizeof(esc_t));
    esc->max_quantum = MAX_QUANTUM;
    esc->ini = 0;
    esc->tam = 0;
    esc->cap = cap;
    esc->tab = (processo_t**)malloc(sizeof(processo_t*) * cap);
    return esc;
}

void escalonador_destroi(esc_t *self) {
    free(self->tab);
    free(self);
}

// TABELA
int escalonador_adiciona_processo(esc_t *self, processo_t *proc) {
    if (self->tam >= self->cap) return -1;
    self->tab[(self->ini + self->tam) % self->cap] = proc;
    self->tam++;
    return 0;
}

int escalonador_busca_processo(esc_t *self, int pid);
void escalonador_remove_processo(esc_t *self, int pid) {
    if (pid == 0) {
        processo_recalcula_priori(self->tab[self->ini]);
        self->tab[self->ini] = NULL;
        self->ini = (self->ini + 1) % self->cap;
        self->tam--;
    }
    else {
        int indice = escalonador_busca_processo(self, pid);
        if (indice == -1) {
            return;
        }
        processo_recalcula_priori(self->tab[indice]);
        self->tam--;
        for (int i = indice; i < (indice + self->tam); i++) {
            self->tab[i % self->cap] = self->tab[(i + 1) % self->cap];
        }
    }
}

void escalonador_manda_fim_fila(esc_t *self) {
    self->tab[(self->ini + self->tam) % self->cap] = self->tab[self->ini];
    self->ini = (self->ini + 1) % self->cap;
}

int escalonador_busca_processo(esc_t *self, int pid) {
    for (int i = self->ini; i < self->ini + self->tam; i++) {
        if (processo_pega_id(self->tab[i % self->cap]) == pid) {
            return i % self->cap;
        }
    }
    return -1;
}

processo_t *escalonador_pega_atual(esc_t *self) {
    if (self->tam == 0) return NULL;
    return self->tab[self->ini];
}

void escalonador_reordena_priori(esc_t *self) {
    // implementacao do insertion sort
    // futuramente da pra trocar pra um nlgn
    for (int i = 1; i < self->tam; i++) {
        processo_t *temp = self->tab[(i + self->ini) % self->cap];
        int j = i;
        while (j > 0 && processo_pega_priori(temp) < processo_pega_priori(self->tab[(j - 1 + self->ini) % self->cap])) {
            self->tab[(j + self->ini) % self->cap] = self->tab[(j - 1 + self->ini) % self->cap];
            j--;
        }
        self->tab[(j + self->ini) % self->cap] = temp;
    }
}

// ESCALONADORES
int escalonador_simples(esc_t *self, so_t *so) {
    if(self->tam == 0 || self->tab[self->ini] == NULL) {
        return 0;
    }
    if (processo_pega_estado(self->tab[self->ini]) == PROC_PRONTO) {
        processo_muda_estado(self->tab[self->ini], PROC_EXECUTANDO);
    }
    return 0;
}

int escalonador_circular(esc_t *self, so_t *so) {
    if(self->tam == 0 || self->tab[self->ini] == NULL) {
        return 0;
    }
    int preemp = 0;
    if (processo_pega_estado(self->tab[self->ini]) == PROC_EXECUTANDO) {
        if (processo_pega_quantum(self->tab[self->ini]) <= 0) {
            processo_reseta_quantum(self->tab[self->ini]);
            processo_muda_estado(self->tab[self->ini], PROC_PRONTO);
            preemp = 1;
            escalonador_manda_fim_fila(self);
        }
    }
    if (processo_pega_estado(self->tab[self->ini]) == PROC_PRONTO) {
        processo_muda_estado(self->tab[self->ini], PROC_EXECUTANDO);
    }
    return preemp;
}

int escalonador_prioridade(esc_t *self, so_t *so) {
    if(self->tam == 0 || self->tab[self->ini] == NULL) {
        return 0;
    }
    int preemp = 0;
    if (processo_pega_estado(self->tab[self->ini]) == PROC_EXECUTANDO) {
        if (processo_pega_quantum(self->tab[self->ini]) <= 0) {
            processo_recalcula_priori(self->tab[self->ini]);
            processo_reseta_quantum(self->tab[self->ini]);
            processo_muda_estado(self->tab[self->ini], PROC_PRONTO);
            preemp = 1;
            escalonador_reordena_priori(self);
        }
    }
    if (processo_pega_estado(self->tab[self->ini]) == PROC_PRONTO) {
        processo_muda_estado(self->tab[self->ini], PROC_EXECUTANDO);
    }
    return preemp;
}
