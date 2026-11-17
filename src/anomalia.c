#include "anomalia.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================================================= lista de anomalias */

ListaAnomalias *anomalias_criar(void)
{
    ListaAnomalias *lista = malloc(sizeof *lista);
    if (lista) {
        lista->inicio = NULL;
        lista->total = 0;
    }
    return lista;
}

Anomalia *anomalias_inserir(ListaAnomalias *lista, const char *pasta, const char *descricao)
{
    if (!lista) return NULL;

    Anomalia *nova = calloc(1, sizeof *nova);
    if (!nova) return NULL;

    nova->id = lista->total + 1;
    snprintf(nova->pasta, sizeof nova->pasta, "%s", pasta ? pasta : "");
    snprintf(nova->descricao, sizeof nova->descricao, "%s",
             (descricao && descricao[0]) ? descricao : "Anomalia sem descrição");

    /* caminha até o último nó e pendura o novo depois dele */
    if (!lista->inicio) {
        lista->inicio = nova;
    } else {
        Anomalia *ultimo = lista->inicio;
        while (ultimo->prox) ultimo = ultimo->prox;
        ultimo->prox = nova;
    }
    lista->total++;
    return nova;
}

static void liberar_aparicoes(Anomalia *anomalia)
{
    Aparicao *ap = anomalia->aparicoes;
    while (ap) {
        Aparicao *seguinte = ap->prox;
        free(ap);
        ap = seguinte;
    }
    anomalia->aparicoes = NULL;
    anomalia->totalAparicoes = 0;
}

bool anomalias_remover(ListaAnomalias *lista, Anomalia *alvo)
{
    if (!lista || !alvo) return false;

    Anomalia *anterior = NULL;
    for (Anomalia *a = lista->inicio; a; anterior = a, a = a->prox) {
        if (a != alvo) continue;
        if (anterior) anterior->prox = a->prox;
        else          lista->inicio = a->prox;
        liberar_aparicoes(a);
        free(a);
        lista->total--;
        return true;
    }
    return false;
}

int anomalias_carregar(ListaAnomalias *lista, const char *caminho)
{
    FILE *arquivo = fopen(caminho, "r");
    if (!lista || !arquivo) {
        if (arquivo) fclose(arquivo);
        return -1;
    }

    char linha[512];
    int lidas = 0;
    Anomalia *anomalia = NULL;      /* a anomalia que as linhas "quadro" completam */
    Aparicao *aparicao = NULL;      /* a aparição que as linhas "area" completam   */

    while (fgets(linha, sizeof linha, arquivo)) {
        char *p = linha;
        if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB &&
            (unsigned char)p[2] == 0xBF) p += 3;          /* marca BOM do UTF-8 */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;

        char pasta[ANOMALIA_PASTA_MAX] = "";
        char descricao[ANOMALIA_DESCRICAO_MAX] = "";
        int quadro;
        float x, y, largura, altura;

        if (sscanf(p, "anomalia %47s %95[^\r\n]", pasta, descricao) >= 1) {
            anomalia = anomalias_inserir(lista, pasta, descricao);
            aparicao = NULL;
            if (anomalia) lidas++;
        } else if (anomalia &&
                   sscanf(p, "quadro %d %f %f %f %f", &quadro, &x, &y, &largura, &altura) == 5 &&
                   largura > 0 && altura > 0) {
            aparicao = anomalia_adicionar_aparicao(anomalia, quadro);
            if (aparicao) aparicao_adicionar_area(aparicao, x, y, largura, altura);
        } else if (aparicao &&
                   sscanf(p, "area %f %f %f %f", &x, &y, &largura, &altura) == 4 &&
                   largura > 0 && altura > 0) {
            aparicao_adicionar_area(aparicao, x, y, largura, altura);
        } else {
            fprintf(stderr, "anomalias: linha ignorada: %s", linha);
        }
    }
    fclose(arquivo);
    return lidas;
}

void anomalias_reiniciar(ListaAnomalias *lista)
{
    if (!lista) return;
    for (Anomalia *a = lista->inicio; a; a = a->prox) {
        a->ativa = false;
        a->desempate = 0;
        a->coletada = false;
        a->ordemColeta = 0;
        a->quadroColeta = 0;
        a->tempoColeta = 0.0;
    }
}

/* 'a' deve ficar na frente de 'b' na fila para escolher um quadro? */
static bool vem_antes(const Anomalia *a, const Anomalia *b)
{
    if (a->ativa != b->ativa) return a->ativa;                 /* ativas na frente */
    if (a->totalAparicoes != b->totalAparicoes)                /* menos opções primeiro */
        return a->totalAparicoes < b->totalAparicoes;
    return a->desempate < b->desempate;
}

/*
 * INSERTION SORT em lista encadeada.
 * Tira os nós um a um da lista original e encaixa cada um na posição certa
 * de uma nova lista que está sempre ordenada. Nenhum dado é copiado: só os
 * ponteiros 'prox' mudam.
 */
void anomalias_ordenar(ListaAnomalias *lista)
{
    if (!lista) return;

    Anomalia *ordenada = NULL;
    Anomalia *atual = lista->inicio;

    while (atual) {
        Anomalia *seguinte = atual->prox;

        if (!ordenada || vem_antes(atual, ordenada)) {
            atual->prox = ordenada;             /* vira a nova cabeça */
            ordenada = atual;
        } else {
            Anomalia *p = ordenada;
            while (p->prox && !vem_antes(atual, p->prox)) p = p->prox;
            atual->prox = p->prox;              /* encaixa depois de p */
            p->prox = atual;
        }
        atual = seguinte;
    }
    lista->inicio = ordenada;
}

int anomalias_sortear(ListaAnomalias *lista, int chance)
{
    if (!lista || lista->total == 0) return 0;

    /* a regra dos 60%: cada anomalia entra ou não na partida */
    int ativas = 0;
    for (Anomalia *a = lista->inicio; a; a = a->prox) {
        a->ativa = (rand() % 100) < chance;
        a->desempate = (unsigned)rand();
        if (a->ativa) ativas++;
    }

    /* um filme sem nenhuma anomalia não teria o que investigar */
    if (ativas == 0) {
        int escolhida = rand() % lista->total;
        Anomalia *a = lista->inicio;
        while (escolhida-- > 0) a = a->prox;
        a->ativa = true;
        ativas = 1;
    }
    return ativas;
}

int anomalias_contar_ativas(const ListaAnomalias *lista)
{
    int n = 0;
    if (lista)
        for (const Anomalia *a = lista->inicio; a; a = a->prox)
            if (a->ativa) n++;
    return n;
}

int anomalias_contar_coletadas(const ListaAnomalias *lista)
{
    int n = 0;
    if (lista)
        for (const Anomalia *a = lista->inicio; a; a = a->prox)
            if (a->ativa && a->coletada) n++;
    return n;
}

void anomalias_liberar(ListaAnomalias *lista)
{
    if (!lista) return;
    Anomalia *a = lista->inicio;
    while (a) {
        Anomalia *seguinte = a->prox;
        liberar_aparicoes(a);
        free(a);
        a = seguinte;
    }
    free(lista);
}

/* ================================================= sub-lista de aparições */

Aparicao *anomalia_adicionar_aparicao(Anomalia *anomalia, int quadro)
{
    if (!anomalia) return NULL;

    /* acha o ponto de inserção que mantém a sub-lista em ordem de quadro */
    Aparicao *anterior = NULL;
    Aparicao *ap = anomalia->aparicoes;
    while (ap && ap->quadro < quadro) {
        anterior = ap;
        ap = ap->prox;
    }
    if (ap && ap->quadro == quadro) return ap;     /* já cadastrada */

    Aparicao *nova = calloc(1, sizeof *nova);
    if (!nova) return NULL;
    nova->quadro = quadro;
    nova->prox = ap;
    if (anterior) anterior->prox = nova;
    else          anomalia->aparicoes = nova;
    anomalia->totalAparicoes++;
    return nova;
}

Aparicao *anomalia_buscar_aparicao(const Anomalia *anomalia, int quadro)
{
    if (!anomalia) return NULL;
    for (Aparicao *ap = anomalia->aparicoes; ap; ap = ap->prox)
        if (ap->quadro == quadro) return ap;
    return NULL;
}

bool anomalia_remover_aparicao(Anomalia *anomalia, Aparicao *alvo)
{
    if (!anomalia || !alvo) return false;

    Aparicao *anterior = NULL;
    for (Aparicao *ap = anomalia->aparicoes; ap; anterior = ap, ap = ap->prox) {
        if (ap != alvo) continue;
        if (anterior) anterior->prox = ap->prox;
        else          anomalia->aparicoes = ap->prox;
        free(ap);
        anomalia->totalAparicoes--;
        return true;
    }
    return false;
}

bool aparicao_adicionar_area(Aparicao *aparicao, float x, float y, float largura, float altura)
{
    if (!aparicao || aparicao->totalAreas >= APARICAO_MAX_AREAS) return false;
    aparicao->areas[aparicao->totalAreas++] = (Area){ x, y, largura, altura };
    return true;
}

bool aparicao_contem_ponto(const Aparicao *aparicao, float px, float py, float folga)
{
    if (!aparicao) return false;
    for (int i = 0; i < aparicao->totalAreas; i++) {
        const Area *r = &aparicao->areas[i];
        if (px >= r->x - folga && px <= r->x + r->largura + folga &&
            py >= r->y - folga && py <= r->y + r->altura + folga)
            return true;
    }
    return false;
}
