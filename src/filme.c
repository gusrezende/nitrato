#include "filme.h"

#include <stdlib.h>

Filme *filme_criar(void)
{
    Filme *filme = malloc(sizeof *filme);
    if (filme) {
        filme->inicio = NULL;
        filme->atual = NULL;
        filme->total = 0;
    }
    return filme;
}

Quadro *filme_inserir(Filme *filme, int numero, Texture2D imagem)
{
    if (!filme) return NULL;

    Quadro *novo = calloc(1, sizeof *novo);
    if (!novo) return NULL;

    novo->numero = numero;
    novo->indice = filme->total;
    novo->imagem = imagem;

    if (!filme->inicio) {
        /* primeiro quadro: aponta para ele mesmo nos dois sentidos */
        novo->ant = novo;
        novo->prox = novo;
        filme->inicio = novo;
        filme->atual = novo;
    } else {
        /* numa lista circular, o último é o anterior do primeiro */
        Quadro *ultimo = filme->inicio->ant;
        novo->ant = ultimo;
        novo->prox = filme->inicio;
        ultimo->prox = novo;
        filme->inicio->ant = novo;
    }
    filme->total++;
    return novo;
}

/* ------------------------------------------------------------------------
 * MERGE SORT em lista encadeada
 *
 * Durante a ordenação a lista é tratada como simplesmente encadeada e
 * linear (só 'prox', terminando em NULL). Depois filme_ordenar() refaz os
 * ponteiros 'ant' e fecha o círculo de novo.
 * ---------------------------------------------------------------------- */

/* Corta a lista ao meio e devolve a cabeça da segunda metade. */
static Quadro *dividir(Quadro *cabeca)
{
    Quadro *lento = cabeca;
    Quadro *rapido = cabeca->prox;

    /* 'rapido' anda dois nós por vez; quando ele chega ao fim, 'lento'
     * está no meio */
    while (rapido && rapido->prox) {
        lento = lento->prox;
        rapido = rapido->prox->prox;
    }
    Quadro *segunda = lento->prox;
    lento->prox = NULL;
    return segunda;
}

/* Intercala duas listas já ordenadas em uma só, também ordenada. */
static Quadro *intercalar(Quadro *a, Quadro *b)
{
    Quadro cabecaFalsa;
    Quadro *cauda = &cabecaFalsa;
    cabecaFalsa.prox = NULL;

    while (a && b) {
        if (a->numero <= b->numero) {   /* <= mantém a ordenação estável */
            cauda->prox = a;
            a = a->prox;
        } else {
            cauda->prox = b;
            b = b->prox;
        }
        cauda = cauda->prox;
    }
    cauda->prox = a ? a : b;            /* sobra de uma das listas */
    return cabecaFalsa.prox;
}

static Quadro *merge_sort(Quadro *cabeca)
{
    if (!cabeca || !cabeca->prox) return cabeca;    /* 0 ou 1 nó: já ordenado */

    Quadro *segunda = dividir(cabeca);
    cabeca = merge_sort(cabeca);
    segunda = merge_sort(segunda);
    return intercalar(cabeca, segunda);
}

void filme_ordenar(Filme *filme)
{
    if (!filme || !filme->inicio) return;

    /* 1. abre o círculo: o último quadro passa a terminar em NULL */
    filme->inicio->ant->prox = NULL;

    /* 2. ordena usando apenas 'prox' */
    filme->inicio = merge_sort(filme->inicio);

    /* 3. refaz 'ant', renumera os índices e fecha o círculo */
    Quadro *anterior = NULL;
    int indice = 0;
    for (Quadro *q = filme->inicio; q; q = q->prox) {
        q->ant = anterior;
        q->indice = indice++;
        anterior = q;
    }
    anterior->prox = filme->inicio;
    filme->inicio->ant = anterior;

    filme->atual = filme->inicio;
}

Quadro *filme_buscar(const Filme *filme, int numero)
{
    if (!filme || !filme->inicio) return NULL;

    Quadro *q = filme->inicio;
    do {
        if (q->numero == numero) return q;
        q = q->prox;
    } while (q != filme->inicio);       /* deu a volta completa: não existe */
    return NULL;
}

Quadro *filme_avancar(Filme *filme)
{
    if (!filme || !filme->atual) return NULL;
    filme->atual = filme->atual->prox;
    return filme->atual;
}

Quadro *filme_retroceder(Filme *filme)
{
    if (!filme || !filme->atual) return NULL;
    filme->atual = filme->atual->ant;
    return filme->atual;
}

void filme_rebobinar(Filme *filme)
{
    if (filme) filme->atual = filme->inicio;
}

Quadro *filme_plantar_anomalia(Filme *filme, Anomalia *anomalia, Aparicao *aparicao)
{
    if (!filme || !anomalia || !aparicao) return NULL;

    Quadro *quadro = filme_buscar(filme, aparicao->quadro);
    if (quadro) {
        quadro->anomalia = anomalia;
        quadro->aparicao = aparicao;
    }
    return quadro;
}

Quadro *filme_plantar_em_quadro_livre(Filme *filme, Anomalia *anomalia)
{
    if (!filme || !anomalia) return NULL;

    /* 1ª passada: quantas aparições caem em quadros que existem e estão livres */
    int livres = 0;
    for (Aparicao *ap = anomalia->aparicoes; ap; ap = ap->prox) {
        Quadro *q = filme_buscar(filme, ap->quadro);
        if (q && !q->anomalia) livres++;
    }
    if (livres == 0) return NULL;

    /* 2ª passada: caminha até a aparição livre sorteada e planta */
    int sorteada = rand() % livres;
    for (Aparicao *ap = anomalia->aparicoes; ap; ap = ap->prox) {
        Quadro *q = filme_buscar(filme, ap->quadro);
        if (q && !q->anomalia && sorteada-- == 0)
            return filme_plantar_anomalia(filme, anomalia, ap);
    }
    return NULL;
}

int filme_remover_anomalia(Filme *filme, const Anomalia *anomalia)
{
    if (!filme || !filme->inicio) return 0;

    int limpos = 0;
    Quadro *q = filme->inicio;
    do {
        if (q->anomalia && (!anomalia || q->anomalia == anomalia)) {
            q->anomalia = NULL;
            q->aparicao = NULL;
            limpos++;
        }
        q = q->prox;
    } while (q != filme->inicio);
    return limpos;
}

void filme_percorrer(Filme *filme, void (*visita)(Quadro *quadro, void *dados), void *dados)
{
    if (!filme || !filme->inicio || !visita) return;

    Quadro *q = filme->inicio;
    do {
        Quadro *seguinte = q->prox;     /* guardado antes, caso a visita mexa no nó */
        visita(q, dados);
        q = seguinte;
    } while (q != filme->inicio);
}

void filme_liberar(Filme *filme)
{
    if (!filme) return;

    if (filme->inicio) {
        filme->inicio->ant->prox = NULL;        /* abre o círculo para ter um fim */
        Quadro *q = filme->inicio;
        while (q) {
            Quadro *seguinte = q->prox;
            free(q);
            q = seguinte;
        }
    }
    free(filme);
}
