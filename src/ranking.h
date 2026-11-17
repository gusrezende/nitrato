/*
 * ranking.h — melhores investigações, guardadas em arquivo de texto.
 * Usa a versão em VETOR do insertion sort (a versão em lista encadeada
 * está em anomalia.c).
 */
#ifndef RANKING_H
#define RANKING_H

#define RANKING_MAX  10
#define NOME_MAX     12

typedef struct Registro {
    char nome[NOME_MAX + 1];
    float tempo;        /* tempo final em segundos, já com as penalidades */
    int pausas;         /* pausas usadas                                  */
    int erros;          /* cliques errados                                */
} Registro;

typedef struct Ranking {
    Registro itens[RANKING_MAX + 1];    /* +1: vaga temporária para inserir */
    int total;
} Ranking;

/* Lê o arquivo (se existir) e deixa o ranking ordenado. */
void ranking_carregar(Ranking *ranking, const char *caminho);

/* Grava o ranking. Devolve 1 se conseguiu. */
int ranking_salvar(const Ranking *ranking, const char *caminho);

/* INSERTION SORT em vetor: menor tempo primeiro; no empate, menos pausas. */
void ranking_ordenar(Ranking *ranking);

/* Insere um resultado e reordena. Devolve a posição (0 = primeiro lugar)
 * ou -1 se o resultado não entrou entre os RANKING_MAX melhores. */
int ranking_inserir(Ranking *ranking, Registro novo);

#endif
