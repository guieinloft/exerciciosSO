#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "so.h"
#include "processo.h"
#include "escalonador.h"
#include "console.h"

struct esc_t {
    esc_tipo_t tipo;
    int ini;
    int tam;
    int cap;
    processo_t **tab;
    int max_quantum;
};

// CRIAÇÃO
esc_t *escalonador_cria(esc_tipo_t tipo, int cap, int max_quantum) {
    esc_t *esc = (esc_t*)malloc(sizeof(esc_t));
    esc->tipo = tipo;
    esc->max_quantum = max_quantum;
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
    if (proc == NULL) return -1;
    self->tab[(self->ini + self->tam) % self->cap] = proc;
    self->tam++;
    return 0;
}

int escalonador_busca_processo(esc_t *self, int pid);
void escalonador_remove_processo(esc_t *self, int pid) {
    int indice;
    if (pid == 0) {
        indice = self->ini;
        processo_recalcula_priori(self->tab[self->ini]);
        processo_reseta_quantum(self->tab[self->ini]);
        self->tab[self->ini] = NULL;
        self->ini = (self->ini + 1) % self->cap;
        self->tam--;
    }
    else {
        indice = escalonador_busca_processo(self, pid);
        if (indice == -1) {
            return;
        }
        processo_recalcula_priori(self->tab[indice]);
        processo_reseta_quantum(self->tab[indice]);
        self->tam--;
        for (int i = indice; i < (indice + self->tam); i++) {
            self->tab[i % self->cap] = self->tab[(i + 1) % self->cap];
        }
    }
}

void escalonador_manda_fim_fila(esc_t *self) {
    processo_recalcula_priori(self->tab[self->ini]);
    processo_reseta_quantum(self->tab[self->ini]);
    self->tab[(self->ini + self->tam) % self->cap] = self->tab[self->ini];
    self->tab[self->ini] = NULL;
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
            preemp = 1;
            processo_muda_estado(self->tab[self->ini], PROC_PRONTO);
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
    console_printf("tam: %d", self->tam);
    int preemp = 0;
    if (processo_pega_estado(self->tab[self->ini]) == PROC_EXECUTANDO) {
        if (processo_pega_quantum(self->tab[self->ini]) <= 0) {
            preemp = 1;
            processo_muda_estado(self->tab[self->ini], PROC_PRONTO);
            escalonador_manda_fim_fila(self);
        }
        else {
            return 0;
        }
    }
    for (int i = self->ini; i < self->ini + self->tam; i++) {
        if (processo_pega_priori(self->tab[i % self->cap]) < processo_pega_priori(self->tab[self->ini])) {
            processo_t *temp = self->tab[self->ini];
            self->tab[self->ini] = self->tab[i % self->cap];
            self->tab[i % self->cap] = temp;
        }
    }
    processo_muda_estado(self->tab[self->ini], PROC_EXECUTANDO);
    return preemp;
}

int escalonador_escalona(esc_t *self, so_t *so) {
    switch (self->tipo) {
        case ESC_SIMPLES:
        return escalonador_simples(self, so);
        case ESC_CIRCULAR:
        return escalonador_circular(self, so);
        case ESC_PRIORIDADE:
        return escalonador_prioridade(self, so);
    }
    return -1;
}
