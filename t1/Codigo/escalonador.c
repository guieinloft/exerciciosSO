#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "so.h"
#include "processo.h"
#include "escalonador.h"
#include "console.h"

#define MAX_QUANTUM 5

struct esc_t {
    int max_quantum;
    tabela_t *tabela;
};

esc_t *escalonador_cria(tabela_t *t) {
    esc_t *esc = (esc_t*)malloc(sizeof(esc_t));
    esc->max_quantum = MAX_QUANTUM;
    esc->tabela = t;
    return esc;
}

void escalonador_simples(esc_t *self, so_t *so) {
    if(tabela_pega_processo(self->tabela, 0) == NULL) {
        return;
    }
    console_printf("tam: %d", tabela_pega_tam(self->tabela));
    for(int i = 0; i <= tabela_pega_tam(self->tabela); i++) {
        processo_t *p = tabela_pega_processo(self->tabela, 0);
        console_printf("%d %d", processo_pega_id(p), processo_pega_estado(p));
        if (processo_pega_estado(p) == PROC_EXECUTANDO) {
            return;
        }
        else if (processo_pega_estado(p) == PROC_PRONTO) {
            processo_muda_estado(p, PROC_EXECUTANDO);
            return;
        }
        tabela_manda_fim_fila(self->tabela);
    }
}

void escalonador_circular(esc_t *self, so_t *so) {
    if(tabela_pega_processo(self->tabela, 0) == NULL) {
        return;
    }
    for(int i = 0; i <= tabela_pega_tam(self->tabela); i++) {
        processo_t *p = tabela_pega_processo(self->tabela, 0);
        if (processo_pega_estado(p) == PROC_EXECUTANDO) {
            if (processo_pega_quantum(p) <= 0) {
                processo_reseta_quantum(p);
                processo_muda_estado(p, PROC_PRONTO);
            }
            else {
                return;
            }
        }
        else if (processo_pega_estado(p) == PROC_PRONTO) {
            processo_muda_estado(p, PROC_EXECUTANDO);
            return;
        }
        tabela_manda_fim_fila(self->tabela);
    }
}

void escalonador_prioridade(esc_t *self, so_t *so) {
    if(tabela_pega_processo(self->tabela, 0) == NULL) {
        return;
    }
    processo_t *p = tabela_pega_processo(self->tabela, 0);
    if (processo_pega_estado(p) == PROC_EXECUTANDO) {
        if (processo_pega_quantum(p) <= 0) {
            processo_recalcula_priori(p);
            processo_reseta_quantum(p);
            processo_muda_estado(p, PROC_PRONTO);
            tabela_reordena_priori(self->tabela);
        }
    }
    for(int i = 0; i < tabela_pega_tam(self->tabela); i++) {
        processo_t *p = tabela_pega_processo(self->tabela, 0);
        if (processo_pega_estado(p) == PROC_EXECUTANDO) {
            return;
        }
        else if (processo_pega_estado(p) == PROC_PRONTO) {
            processo_muda_estado(p, PROC_EXECUTANDO);
            return;
        }
        tabela_manda_fim_fila(self->tabela);
    }
}
