#include "ranking.h"

#include <stdio.h>
#include <string.h>

/* 'a' deve ficar na frente de 'b'? */
static int melhor_que(const Registro *a, const Registro *b)
{
    if (a->tempo != b->tempo) return a->tempo < b->tempo;
    if (a->pausas != b->pausas) return a->pausas < b->pausas;
    return a->erros < b->erros;
}

void ranking_ordenar(Ranking *ranking)
{
    /* insertion sort: o trecho [0, i) já está ordenado; o item i é
     * empurrado para a esquerda até achar o lugar dele */
    for (int i = 1; i < ranking->total; i++) {
        Registro chave = ranking->itens[i];
        int j = i - 1;
        while (j >= 0 && melhor_que(&chave, &ranking->itens[j])) {
            ranking->itens[j + 1] = ranking->itens[j];
            j--;
        }
        ranking->itens[j + 1] = chave;
    }
}

void ranking_carregar(Ranking *ranking, const char *caminho)
{
    ranking->total = 0;

    FILE *arquivo = fopen(caminho, "r");
    if (!arquivo) return;               /* primeira execução: ranking vazio */

    char linha[128];
    while (ranking->total < RANKING_MAX && fgets(linha, sizeof linha, arquivo)) {
        Registro r;
        memset(&r, 0, sizeof r);
        /* formato: tempo;pausas;erros;nome */
        if (sscanf(linha, "%f;%d;%d;%12[^\r\n]", &r.tempo, &r.pausas, &r.erros, r.nome) == 4)
            ranking->itens[ranking->total++] = r;
    }
    fclose(arquivo);
    ranking_ordenar(ranking);
}

int ranking_salvar(const Ranking *ranking, const char *caminho)
{
    FILE *arquivo = fopen(caminho, "w");
    if (!arquivo) return 0;

    for (int i = 0; i < ranking->total; i++) {
        const Registro *r = &ranking->itens[i];
        fprintf(arquivo, "%.2f;%d;%d;%s\n", r->tempo, r->pausas, r->erros, r->nome);
    }
    fclose(arquivo);
    return 1;
}

int ranking_inserir(Ranking *ranking, Registro novo)
{
    /* a vaga extra do vetor recebe o novo resultado; depois de ordenar,
     * quem sobrar na última posição fica de fora */
    ranking->itens[ranking->total++] = novo;
    ranking_ordenar(ranking);
    if (ranking->total > RANKING_MAX) ranking->total = RANKING_MAX;

    for (int i = 0; i < ranking->total; i++) {
        const Registro *r = &ranking->itens[i];
        if (r->tempo == novo.tempo && r->pausas == novo.pausas &&
            r->erros == novo.erros && strcmp(r->nome, novo.nome) == 0)
            return i;
    }
    return -1;
}
