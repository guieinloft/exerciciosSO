#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "gerador.h"

struct gerador_t {
    int n_ant;
};

gerador_t *gerador_cria(void) {
    gerador_t *self = (gerador_t*)malloc(sizeof(gerador_t));
    assert(self != NULL);
    srand(time(0));
    return self;
}

void gerador_destroi(gerador_t *self) {
    free(self);
}

int gerador_numero(gerador_t *self) {
    int n_novo = rand();
    self->n_ant = n_novo;
    return n_novo;
}

err_t gerador_leitura(void *disp, int id, int *pvalor) {
    gerador_t *self = disp;
    err_t err = ERR_OK;
    switch (id) {
        case 0:
            *pvalor = gerador_numero(self);
            break;
        default:
            err = ERR_END_INV;
    }
    return err;
}
