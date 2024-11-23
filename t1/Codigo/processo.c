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

struct tabela_t {
    int ini;
    int tam;
    int cap;
    processo_t ** tab;
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
    return 0;
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
    self->priori = (self->priori + (double)self->quantum / self->max_quantum) / 2.0;
}
double processo_pega_priori(processo_t *self) {
    if (self == NULL) return 0.0;
    return self->priori;
}

void processo_mata(processo_t *self) {
    free(self);
}

// TABELA {{{1
tabela_t *tabela_cria(int cap) {
    tabela_t *t = (tabela_t*)malloc(sizeof(tabela_t));
    t->ini = 0;
    t->tam = 0;
    t->cap = cap;
    t->tab = (processo_t**)malloc(sizeof(processo_t*) * cap);
    return t;
}

int tabela_adiciona_processo(tabela_t *self, processo_t *proc) {
    if (self->tam >= self->cap) return -1;
    self->tab[(self->ini + self->tam) % self->cap] = proc;
    self->tam++;
    return 0;
}

void tabela_manda_fim_fila(tabela_t *self) {
    self->tab[(self->ini + self->tam) % self->cap] = self->tab[self->ini];
    self->ini = (self->ini + 1) % self->cap;
}

int tabela_busca_processo(tabela_t *self, int pid) {
    for (int i = self->ini; i < self->ini + self->tam; i++) {
        if (processo_pega_id(self->tab[i % self->cap]) == pid) {
            return i % self->cap;
        }
    }
    return -1;
}

processo_t *tabela_remove_processo(tabela_t *self, int pid) {
    processo_t *proc;
    if (pid == 0) {
        proc = self->tab[self->ini];
        self->tab[self->ini] = NULL;
        self->ini = (self->ini + 1) % self->cap;
        self->tam--;
    }
    else {
        int indice = tabela_busca_processo(self, pid);
        if (indice == -1) {
            return NULL;
        }
        proc = self->tab[indice];
        self->tam--;
        for (int i = indice; i < (indice + self->tam); i++) {
            self->tab[i % self->cap] = self->tab[(i + 1) % self->cap];
        }
    }
    return proc;
}

processo_t *tabela_pega_processo(tabela_t *self, int pid) {
    if (pid == 0) return self->tab[self->ini];
    int indice = tabela_busca_processo(self, pid);
    if (indice == -1) return NULL;
    return self->tab[indice];
}

processo_t *tabela_pega_processo_indice(tabela_t *self, int indice) {
    return self->tab[(indice + self->ini) % self->cap];
}

int tabela_pega_tam(tabela_t *self) {
    return self->tam;
}

void tabela_reordena_priori(tabela_t *self) {
    // implementacao do insertion sort
    // futuramente da pra trocar pra um nlgn
    for (int i = 1; i < self->tam; i++) {
        processo_t *temp = self->tab[(i + self->ini) % self->cap];
        int j = i;
        while (j > 0 && processo_pega_priori(temp) >= processo_pega_priori(self->tab[(j - 1 + self->ini) % self->cap])) {
            self->tab[(j + self->ini) % self->cap] = self->tab[(j - 1 + self->ini) % self->cap];
            j--;
        }
        self->tab[(j + self->ini) % self->cap] = temp;
    }
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
