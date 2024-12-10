#include <stdlib.h>
#include <stdbool.h>

#include "mem_quadro.h"

typedef struct quadro {
    bool livre; // se a posicao da memoria esta livre ou nao
    int dono; // id do processo que ocupa a posicao da memoria
    int pagina; // pagina do processo
} quadro;

typedef struct fila {
    int indice;
    bool r;
} fila;

struct mem_quadros_t {
    int cap;
    quadro *quadros;

    // fila
    int f_ini;
    int f_tam;
    fila *f;
};

mem_quadros_t *mem_quadros_cria(int cap, int quadro_livre) {
    mem_quadros_t *mq = (mem_quadros_t*)malloc(sizeof(mem_quadros_t));
    mq->quadros = (quadro*)malloc(sizeof(quadro) * cap);
    mq->cap = cap;
    for (int i = 0; i < cap; i++) {
        if (i < quadro_livre) {
            mq->quadros[i].livre = 0;
            mq->quadros[i].dono = -1; // restrito
            mq->quadros[i].pagina = 0;
        }
        else {
            mq->quadros[i].livre = 1;
            mq->quadros[i].dono = 0;
            mq->quadros[i].pagina = 0;
        }
    }

    mq->f_ini = 0;
    mq->f_tam = 0;
    mq->f = (fila*)malloc(sizeof(fila) * cap);
    return mq;
}

int mem_quadros_tem_livre(mem_quadros_t *self) {
    for (int i = 0; i < self->cap; i++) {
        if (self->quadros[i].livre) return i;
    }
    return -1;
}

int mem_quadros_adiciona_fila(mem_quadros_t *self, int indice) {
    if (self->f_tam >= self->cap) return -1;
    self->f[(self->f_ini + self->f_tam) % self->cap].indice = indice;
    self->f_tam++;
    return 0;
}

void mem_quadros_remove_fila(mem_quadros_t *self, int indice) {
    if (indice == -1) {
        self->f_ini = (self->f_ini + 1) % self->cap;
        self->f_tam--;
    }
    for (int i = self->f_ini; i < self->f_ini + self->f_tam; i++) {
        if (self->f[i % self->cap].indice == indice) {
            self->f_tam--;
            for (int j = i; j < (i + self->f_tam); j++) {
                self->f[j % self->cap] = self->f[(j + 1) % self->cap];
            }
            break;
        }
    }
}

void mem_quadros_muda_estado(mem_quadros_t *self, int indice, bool livre, int dono) {
    self->quadros[indice].livre = livre;
    self->quadros[indice].dono = dono;
    if (livre) {
        mem_quadros_remove_fila(self, indice);
    }
    else {
        mem_quadros_adiciona_fila(self, indice);
    }
}

int mem_quadros_libera_quadro(mem_quadros_t *self) { 
    int indice = self->f[self->f_ini].indice;
    mem_quadros_remove_fila(self, -1);
    self->quadros[indice].livre = 1;
    return indice;
}

int mem_quadros_pega_dono(mem_quadros_t *self, int indice) {
    return self->quadros[indice].dono;
}
int mem_quadros_pega_pagina(mem_quadros_t *self, int indice) {
    return self->quadros[indice].pagina;
}
