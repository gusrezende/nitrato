/*
 * anomalia.h — LISTA SIMPLESMENTE ENCADEADA de anomalias (o "dossiê" do caso).
 *
 * Cada nó é uma anomalia do filme. Cada anomalia guarda uma segunda lista
 * encadeada, a das suas APARIÇÕES: os quadros alterados em que ela pode ser
 * vista, com a imagem e as áreas clicáveis de cada um.
 *
 *   dossiê -> [sem cauda] -> [rostos] -> [caracol] -> ...
 *                  |             |           |
 *              quadro 1      quadro 11    quadro 1
 *              quadro 2                   quadro 2
 *                ...                        ...
 *
 * A lista participa da lógica do jogo: no começo de cada partida,
 * anomalias_sortear() decide quais anomalias estão escondidas no filme
 * daquela vez. Depois a lista é ORDENADA (insertion sort) para que as
 * anomalias com menos quadros possíveis escolham primeiro o quadro em que
 * vão ficar — assim duas anomalias nunca disputam o mesmo quadro.
 *
 * Este módulo só usa da raylib o tipo Texture2D (nenhuma função dela), então
 * pode ser testado no terminal.
 */
#ifndef ANOMALIA_H
#define ANOMALIA_H

#include <stdbool.h>
#include "raylib.h"

#define ANOMALIA_DESCRICAO_MAX 96
#define ANOMALIA_PASTA_MAX     48
#define APARICAO_MAX_AREAS     4

/* Retângulo clicável, em pixels da imagem do frame. */
typedef struct Area {
    float x, y, largura, altura;
} Area;

/* Um quadro alterado: a anomalia vista em um quadro específico do filme. */
typedef struct Aparicao {
    int quadro;                         /* número do quadro que ela substitui */
    Texture2D imagem;                   /* o frame alterado                   */
    Area areas[APARICAO_MAX_AREAS];     /* onde o jogador pode clicar         */
    int totalAreas;
    struct Aparicao *prox;
} Aparicao;

typedef struct Anomalia {
    int id;                             /* ordem de cadastro (1, 2, 3...)     */
    char pasta[ANOMALIA_PASTA_MAX];     /* pasta com as imagens               */
    char descricao[ANOMALIA_DESCRICAO_MAX];

    Aparicao *aparicoes;                /* sub-lista, em ordem de quadro      */
    int totalAparicoes;

    bool ativa;                         /* foi sorteada para esta partida?    */
    unsigned desempate;                 /* número sorteado a cada partida     */

    bool coletada;                      /* o jogador já clicou nela?          */
    int ordemColeta;                    /* 1 = primeira a ser achada          */
    int quadroColeta;                   /* em que quadro foi achada           */
    double tempoColeta;                 /* em que momento da partida (s)      */

    struct Anomalia *prox;
} Anomalia;

typedef struct ListaAnomalias {
    Anomalia *inicio;
    int total;
} ListaAnomalias;

/* ------------------------------------------------------- lista de anomalias */

/* Cria uma lista vazia. */
ListaAnomalias *anomalias_criar(void);

/* Insere uma anomalia (ainda sem aparições) no fim da lista. */
Anomalia *anomalias_inserir(ListaAnomalias *lista, const char *pasta, const char *descricao);

/* Remove um nó, liberando também as aparições dele. */
bool anomalias_remover(ListaAnomalias *lista, Anomalia *alvo);

/* Lê as anomalias de um arquivo de texto. Devolve quantas foram lidas,
 * ou -1 se o arquivo não pôde ser aberto. */
int anomalias_carregar(ListaAnomalias *lista, const char *caminho);

/* Prepara a lista para uma nova partida (ninguém coletado, contagens zeradas). */
void anomalias_reiniciar(ListaAnomalias *lista);

/* Sorteio da partida: cada anomalia tem 'chance'% (0 a 100) de ficar ativa.
 * Garante ao menos uma. Também sorteia o número de desempate de cada nó.
 * Devolve quantas ficaram ativas. */
int anomalias_sortear(ListaAnomalias *lista, int chance);

/* INSERTION SORT na lista encadeada. Ordem: ativas antes das inativas; entre
 * as ativas, a que tem menos aparições primeiro; empate decidido pelo número
 * 'desempate' de cada nó. */
void anomalias_ordenar(ListaAnomalias *lista);

/* Contagens usadas pelo jogo. */
int anomalias_contar_ativas(const ListaAnomalias *lista);
int anomalias_contar_coletadas(const ListaAnomalias *lista);

/* Libera a lista inteira (as texturas devem ser descarregadas antes). */
void anomalias_liberar(ListaAnomalias *lista);

/* ------------------------------------------------- sub-lista de aparições */

/* Insere uma aparição na anomalia, mantendo a sub-lista em ordem de quadro.
 * Se o quadro já existir, devolve a aparição existente. */
Aparicao *anomalia_adicionar_aparicao(Anomalia *anomalia, int quadro);

/* Procura a aparição de um quadro. Devolve NULL se não existir. */
Aparicao *anomalia_buscar_aparicao(const Anomalia *anomalia, int quadro);

/* Remove uma aparição da anomalia. */
bool anomalia_remover_aparicao(Anomalia *anomalia, Aparicao *alvo);

/* Acrescenta uma área clicável. Devolve false se já houver o máximo. */
bool aparicao_adicionar_area(Aparicao *aparicao, float x, float y, float largura, float altura);

/* O ponto (px, py), em pixels da imagem, cai em alguma área da aparição?
 * 'folga' aumenta as áreas em todos os lados para o clique não ser injusto. */
bool aparicao_contem_ponto(const Aparicao *aparicao, float px, float py, float folga);

#endif
