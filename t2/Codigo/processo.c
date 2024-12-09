// INCLUDES {{{1
#include <stdlib.h>
#include "processo.h"

// STRUCTS {{{1
struct processo_t {
    int id;
    int a;
    int x;
    int pc;
    proc_estado_t estado;
    proc_bloqueio_t bloq_motivo;
    int max_quantum;
    int quantum;
    double priori;
    proc_metricas_t metricas;
};

struct historico_t {
    int id;
    proc_metricas_t metricas;
    struct historico_t *prox;
};

// PROCESSOS {{{1
void metricas_inicializa(processo_t *self);

processo_t *processo_cria(int id, int pc, int max_quantum) {
    processo_t *p = (processo_t*)malloc(sizeof(processo_t));
    if (p == NULL) return NULL;
    p->id = id;
    p->pc = pc;
    p->estado = PROC_PRONTO;
    metricas_inicializa(p);
    p->max_quantum = max_quantum;
    p->quantum = max_quantum;
    p->priori = 0.5;
    return p;
}

void processo_muda_estado(processo_t *self, proc_estado_t e_novo) {
    if (self->estado == PROC_EXECUTANDO && e_novo == PROC_PRONTO) {
        self->metricas.n_preempcoes++;
    }
    self->estado = e_novo;
    self->metricas.estado_vezes[e_novo]++;
}
proc_estado_t processo_pega_estado(processo_t *self) {
    if (self == NULL) return -1;
    return self->estado;
}

void processo_bloqueia(processo_t *self, proc_bloqueio_t motivo) {
    processo_muda_estado(self, PROC_BLOQUEADO);
    self->bloq_motivo = motivo;
}
proc_bloqueio_t processo_pega_bloq_motivo(processo_t *self) {
    return self->bloq_motivo;
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
    if (self != NULL) return self->pc;
    return 0;
}
int processo_pega_reg_a(processo_t *self) {
    if (self != NULL) return self->a;
    return 0;
}
int processo_pega_reg_x(processo_t *self) {
    if (self != NULL) return self->x;
    return 0;
}
int processo_pega_id(processo_t *self) {
    if (self != NULL) return self->id;
    return -1;
}

void processo_muda_quantum(processo_t *self, int quantum) {
    if (self == NULL) return;
    self->quantum = quantum;
}
void processo_decrementa_quantum(processo_t *self) {
    if (self == NULL) return;
    self->quantum--;
}
int processo_pega_quantum(processo_t *self) {
    if (self != NULL) return self->quantum;
    return 0;
}
void processo_reseta_quantum(processo_t *self) {
    if (self == NULL) return;
    self->quantum = self->max_quantum;
}

void processo_muda_priori(processo_t *self, double priori) {
    if (self == NULL) return;
    self->priori = priori;
}
void processo_recalcula_priori(processo_t *self) {
    if (self == NULL) return;
    self->priori = (self->priori + (double)(self->max_quantum - self->quantum) / self->max_quantum) / 2.0;
}
double processo_pega_priori(processo_t *self) {
    if (self == NULL) return 0.0;
    return self->priori;
}

void processo_mata(processo_t *self) {
    free(self);
}

// METRICAS {{{1
void metricas_inicializa(processo_t *self) {
    self->metricas.n_preempcoes = 0;
    self->metricas.tempo_retorno = 0;
    self->metricas.tempo_resposta = 0;
    for (int i = 0; i < N_PROC_ESTADO; i++) {
        self->metricas.estado_vezes[i] = 0;
        self->metricas.estado_tempo[i] = 0;
    }
    self->metricas.estado_vezes[PROC_PRONTO] = 1;
}
    

void processo_atualiza_metricas(processo_t *self, int delta) {
    self->metricas.tempo_retorno += delta;
    self->metricas.estado_tempo[self->estado] += delta;
    self->metricas.tempo_resposta = self->metricas.estado_tempo[PROC_PRONTO] / self->metricas.estado_vezes[PROC_PRONTO];
}

// HISTORICO {{{1

historico_t *historico_adiciona_metrica(historico_t *hist, processo_t *proc) {
    if (proc == NULL) return hist;
    historico_t *novo = (historico_t*)malloc(sizeof(historico_t));
    novo->id = proc->id;
    novo->metricas = proc->metricas;
    novo->prox = hist;
    return novo;
}

int historico_pega_id(historico_t *hist) {
    if (hist == NULL) return 0;
    return hist->id;
}

proc_metricas_t historico_pega_metricas(historico_t *hist) {
    return hist->metricas;
}

void historico_apaga(historico_t *hist) {
    historico_t *h = hist;
    historico_t *h_ant;
    while (h != NULL) {
        h_ant = h;
        h = h->prox;
        free(h_ant);
    }
}

historico_t *historico_prox(historico_t *hist) {
    if (hist != NULL) {
        return hist->prox;
    }
    return NULL;
}