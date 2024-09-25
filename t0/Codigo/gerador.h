//dispositivo de entrada que gera um numero aleatorio

#ifndef GERADOR_H
#define GERADOR_H

#include "err.h"

typedef struct gerador_t gerador_t;

//cria gerador
gerador_t *gerador_cria(void);

//destroi gerador
void gerador_destroi(gerador_t *self);

//funcao para acessar o gerador
err_t gerador_leitura(void *disp, int id, int *pvalor);

#endif
