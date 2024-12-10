#ifndef MEM_QUADRO_H
#define MEM_QUADRO_H

typedef struct mem_quadros_t mem_quadros_t;

mem_quadros_t *mem_quadros_cria(int cap, int quadro_livre);
int mem_quadros_tem_livre(mem_quadros_t *self);
void mem_quadros_muda_estado(mem_quadros_t *self, int indice, bool livre, int dono);
int mem_quadros_libera_quadro(mem_quadros_t *self);
int mem_quadros_pega_dono(mem_quadros_t *self, int indice);
int mem_quadros_pega_pagina(mem_quadros_t *self, int indice);

#endif
