#include <stdlib.h>
#include "processo.h"

struct processo_t {
    int id;
    int a;
    int x;
    int pc;
    proc_estado_t estado;
};

processo_t *processo_cria(int id, int pc) {
    processo_t *p = (processo_t*)malloc(sizeof(processo_t));
    p->id = id;
    p->pc = pc;
    p->estado = PROC_PRONTO;
    return p;
}

void processo_muda_estado(processo_t *self, proc_estado_t e_novo) {
    self->estado = e_novo;
}
proc_estado_t processo_pega_estado(processo_t *self) {
    return self->estado;
}

void processo_salva_reg_pc(processo_t *self, int pc) {
    self->pc = pc;
}
void processo_salva_reg_a(processo_t *self, int a) {
    self->a = a;
}
void processo_salva_reg_x(processo_t *self, int x) {
    self->x = x;
}

int processo_pega_reg_pc(processo_t *self) {
    return self->pc;
}
int processo_pega_reg_a(processo_t *self) {
    return self->a;
}
int processo_pega_reg_x(processo_t *self) {
    return self->x;
}
int processo_pega_id(processo_t *self) {
    return self->id;
}

void processo_mata(processo_t *self) {
    free(self);
}
