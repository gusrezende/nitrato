/*
 * filme.h — LISTA DUPLAMENTE ENCADEADA CIRCULAR: o rolo de filme.
 *
 * Cada nó é um quadro (frame) da animação. O "projetor" é um ponteiro
 * ('atual') que caminha pela lista:
 *
 *      PLAY / AVANÇAR  ->  atual = atual->prox
 *      RETROCEDER      ->  atual = atual->ant
 *
 * A lista é circular porque o rolo recomeça quando chega ao fim (e, ao
 * retroceder do primeiro quadro, cai no último). É duplamente encadeada
 * porque o jogador pode rodar o filme nos dois sentidos.
 *
 * Os quadros chegam do disco em ordem qualquer; quem coloca o filme na
 * ordem certa é o MERGE SORT de filme_ordenar().
 *
 * Este módulo só usa da raylib o tipo Texture2D (nenhuma função dela).
 */
#ifndef FILME_H
#define FILME_H

#include "raylib.h"
#include "anomalia.h"

typedef struct Quadro {
    int numero;                 /* número do quadro (tirado do nome do arquivo) */
    int indice;                 /* posição no rolo depois de ordenado (0, 1...) */

    Texture2D imagem;           /* versão normal do quadro                      */

    Anomalia *anomalia;         /* anomalia plantada neste quadro, ou NULL      */
    Aparicao *aparicao;         /* a versão alterada que deve ser exibida       */

    struct Quadro *ant;
    struct Quadro *prox;
} Quadro;

typedef struct Filme {
    Quadro *inicio;             /* primeiro quadro do rolo                      */
    Quadro *atual;              /* quadro que está na frente do projetor        */
    int total;
} Filme;

/* Cria um rolo vazio. */
Filme *filme_criar(void);

/* Insere um quadro no fim do rolo, mantendo a lista circular. */
Quadro *filme_inserir(Filme *filme, int numero, Texture2D imagem);

/* MERGE SORT: coloca os quadros em ordem crescente de número e refaz os
 * índices. O projetor volta para o primeiro quadro. */
void filme_ordenar(Filme *filme);

/* Procura um quadro pelo número. Devolve NULL se não existir. */
Quadro *filme_buscar(const Filme *filme, int numero);

/* Move o projetor um quadro para a frente / para trás e devolve o novo atual. */
Quadro *filme_avancar(Filme *filme);
Quadro *filme_retroceder(Filme *filme);

/* Leva o projetor de volta ao primeiro quadro. */
void filme_rebobinar(Filme *filme);

/* Planta uma aparição da anomalia no quadro que ela substitui: enquanto
 * estiver plantada, esse quadro exibe a imagem alterada. Devolve o quadro,
 * ou NULL se o filme não tiver um quadro com aquele número. */
Quadro *filme_plantar_anomalia(Filme *filme, Anomalia *anomalia, Aparicao *aparicao);

/* Escolhe ao acaso, entre as aparições da anomalia, uma cujo quadro ainda
 * não tenha outra anomalia, e planta. Devolve o quadro, ou NULL se todos os
 * quadros possíveis já estiverem ocupados. */
Quadro *filme_plantar_em_quadro_livre(Filme *filme, Anomalia *anomalia);

/* Tira a anomalia do filme (quando é coletada ou quando a partida recomeça).
 * Passe NULL para limpar todas. Devolve quantos quadros foram limpos. */
int filme_remover_anomalia(Filme *filme, const Anomalia *anomalia);

/* Chama 'visita' para cada quadro, do primeiro ao último. */
void filme_percorrer(Filme *filme, void (*visita)(Quadro *quadro, void *dados), void *dados);

/* Libera todos os nós (as texturas devem ser descarregadas antes). */
void filme_liberar(Filme *filme);

#endif
