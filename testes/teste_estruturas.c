/*
 * teste_estruturas.c — testa no terminal as listas e as ordenações, sem
 * abrir janela.  Compile com:  make teste
 */
#include "../src/anomalia.h"
#include "../src/filme.h"
#include "../src/ranking.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas = 0;

#define CONFERIR(condicao, descricao)                                   \
    do {                                                                \
        if (condicao) printf("  ok     %s\n", descricao);               \
        else { printf("  FALHOU %s\n", descricao); falhas++; }          \
    } while (0)

static void contar_visita(Quadro *quadro, void *dados)
{
    (void)quadro;
    (*(int *)dados)++;
}

static void testar_filme(void)
{
    printf("\nROLO DE FILME (lista duplamente encadeada circular + merge sort)\n");

    const int N = 132;
    Texture2D vazia = { 0 };
    Filme *filme = filme_criar();

    /* insere os quadros 1..N fora de ordem, como o sistema de arquivos faria */
    int numeros[132];
    for (int i = 0; i < N; i++) numeros[i] = i + 1;
    for (int i = N - 1; i > 0; i--) {
        int k = rand() % (i + 1);
        int troca = numeros[i]; numeros[i] = numeros[k]; numeros[k] = troca;
    }
    for (int i = 0; i < N; i++) filme_inserir(filme, numeros[i], vazia);
    CONFERIR(filme->total == N, "132 quadros inseridos");

    filme_ordenar(filme);

    bool crescente = true, indices = true, voltaInteira = true;
    Quadro *q = filme->inicio;
    for (int i = 0; i < N; i++, q = q->prox) {
        if (q->numero != i + 1) crescente = false;
        if (q->indice != i) indices = false;
        if (q->prox->ant != q || q->ant->prox != q) voltaInteira = false;
    }
    CONFERIR(crescente, "merge sort deixou os quadros em ordem crescente");
    CONFERIR(indices, "índices renumerados de 0 a 131");
    CONFERIR(voltaInteira && q == filme->inicio, "ponteiros ant/prox coerentes e círculo fechado");

    filme_rebobinar(filme);
    CONFERIR(filme_retroceder(filme)->numero == N, "retroceder do primeiro quadro cai no último");
    CONFERIR(filme_avancar(filme)->numero == 1, "avançar do último quadro volta ao primeiro");
    for (int i = 0; i < 10; i++) filme_avancar(filme);
    for (int i = 0; i < 4; i++) filme_retroceder(filme);
    CONFERIR(filme->atual->numero == 7, "10 passos à frente e 4 atrás param no quadro 7");

    CONFERIR(filme_buscar(filme, 77) && filme_buscar(filme, 77)->numero == 77, "busca acha o quadro 77");
    CONFERIR(filme_buscar(filme, 999) == NULL, "busca devolve NULL para quadro inexistente");

    Anomalia a = { 0 };
    Aparicao *ap = anomalia_adicionar_aparicao(&a, 44);
    Quadro *plantado = filme_plantar_anomalia(filme, &a, ap);
    CONFERIR(plantado && plantado->numero == 44 && plantado->anomalia == &a && plantado->aparicao == ap,
             "aparição plantada exatamente no quadro 44");
    CONFERIR(filme_buscar(filme, 43)->anomalia == NULL && filme_buscar(filme, 45)->anomalia == NULL,
             "os quadros vizinhos continuam normais");
    CONFERIR(filme_remover_anomalia(filme, &a) == 1 && plantado->anomalia == NULL && plantado->aparicao == NULL,
             "anomalia retirada do filme");
    Aparicao *fora = anomalia_adicionar_aparicao(&a, 999);
    CONFERIR(filme_plantar_anomalia(filme, &a, fora) == NULL, "não planta em quadro que o filme não tem");
    anomalia_remover_aparicao(&a, fora);
    anomalia_remover_aparicao(&a, ap);

    /* duas anomalias nunca dividem um quadro: 'so10' só existe no quadro 10;
     * 'duas' existe no 10 e no 20 e, com o 10 ocupado, tem de ficar no 20 */
    Anomalia so10 = { 0 }, duas = { 0 }, tambem10 = { 0 };
    anomalia_adicionar_aparicao(&so10, 10);
    anomalia_adicionar_aparicao(&duas, 10);
    anomalia_adicionar_aparicao(&duas, 20);
    anomalia_adicionar_aparicao(&tambem10, 10);
    Quadro *q10 = filme_plantar_em_quadro_livre(filme, &so10);
    Quadro *q20 = filme_plantar_em_quadro_livre(filme, &duas);
    CONFERIR(q10 && q10->numero == 10 && q20 && q20->numero == 20,
             "cada anomalia fica em um quadro livre diferente");
    CONFERIR(filme_plantar_em_quadro_livre(filme, &tambem10) == NULL,
             "sem quadro livre, a anomalia não é plantada");
    CONFERIR(filme_remover_anomalia(filme, NULL) == 2, "limpeza geral do filme");
    bool espalhou10 = false, espalhou20 = false;
    for (int i = 0; i < 200; i++) {
        Quadro *q = filme_plantar_em_quadro_livre(filme, &duas);
        if (q->numero == 10) espalhou10 = true;
        if (q->numero == 20) espalhou20 = true;
        filme_remover_anomalia(filme, &duas);
    }
    CONFERIR(espalhou10 && espalhou20, "com os dois quadros livres, o sorteio usa ambos");
    while (so10.aparicoes) anomalia_remover_aparicao(&so10, so10.aparicoes);
    while (duas.aparicoes) anomalia_remover_aparicao(&duas, duas.aparicoes);
    while (tambem10.aparicoes) anomalia_remover_aparicao(&tambem10, tambem10.aparicoes);

    int visitados = 0;
    filme_percorrer(filme, contar_visita, &visitados);
    CONFERIR(visitados == N, "percorrer visita cada quadro uma vez");

    filme_liberar(filme);
}

static void testar_anomalias(void)
{
    printf("\nDOSSIÊ (lista encadeada de anomalias, cada uma com sua lista de aparições)\n");

    ListaAnomalias *lista = anomalias_criar();
    Anomalia *a1 = anomalias_inserir(lista, "a1", "Primeira");
    anomalias_inserir(lista, "a2", "Segunda");
    anomalias_inserir(lista, "a3", "Terceira");
    anomalias_inserir(lista, "a4", "Quarta");
    CONFERIR(lista->total == 4, "4 anomalias inseridas");

    /* sub-lista de aparições: inserção ordenada */
    Aparicao *q7 = anomalia_adicionar_aparicao(a1, 7);
    anomalia_adicionar_aparicao(a1, 2);
    anomalia_adicionar_aparicao(a1, 5);
    CONFERIR(a1->totalAparicoes == 3 && a1->aparicoes->quadro == 2 &&
             a1->aparicoes->prox->quadro == 5 && a1->aparicoes->prox->prox->quadro == 7,
             "aparições inseridas fora de ordem ficam em ordem de quadro");
    CONFERIR(anomalia_adicionar_aparicao(a1, 7) == q7 && a1->totalAparicoes == 3,
             "quadro repetido não cria aparição duplicada");
    CONFERIR(anomalia_buscar_aparicao(a1, 5) && !anomalia_buscar_aparicao(a1, 6), "busca de aparição por quadro");

    aparicao_adicionar_area(q7, 10, 10, 20, 20);
    aparicao_adicionar_area(q7, 100, 50, 30, 30);
    CONFERIR(aparicao_contem_ponto(q7, 15, 15, 0) && aparicao_contem_ponto(q7, 120, 60, 0) &&
             !aparicao_contem_ponto(q7, 60, 30, 8), "clique vale em qualquer área da aparição e só nelas");

    CONFERIR(anomalia_remover_aparicao(a1, anomalia_buscar_aparicao(a1, 5)) && a1->totalAparicoes == 2 &&
             a1->aparicoes->prox == q7, "remoção de aparição religou a sub-lista");

    /* a2 fica com 1 aparição, a3 com 3, a4 com 2 (a1 já tem 2) */
    Anomalia *a2 = a1->prox, *a3 = a2->prox, *a4 = a3->prox;
    anomalia_adicionar_aparicao(a2, 11);
    for (int q = 1; q <= 3; q++) anomalia_adicionar_aparicao(a3, q);
    for (int q = 1; q <= 2; q++) anomalia_adicionar_aparicao(a4, q);

    /* sorteio da partida */
    CONFERIR(anomalias_sortear(lista, 100) == 4 && anomalias_contar_ativas(lista) == 4,
             "com 100% de chance, todas entram na partida");
    CONFERIR(anomalias_sortear(lista, 0) == 1 && anomalias_contar_ativas(lista) == 1,
             "com 0% de chance, ainda entra uma (mínimo garantido)");

    /* ordenação: ativas na frente, e entre elas a de menos aparições primeiro */
    anomalias_sortear(lista, 100);
    a3->ativa = false;
    anomalias_ordenar(lista);
    Anomalia *o = lista->inicio;
    CONFERIR(o == a2 && o->prox->totalAparicoes == 2 && o->prox->prox->totalAparicoes == 2 &&
             o->prox->prox->prox == a3 && a3->prox == NULL,
             "insertion sort: ativas primeiro, da mais limitada para a mais livre");

    /* a regra dos 60%: em muitas partidas, a média de ativas fica perto de 60% */
    const int PARTIDAS = 20000;
    long ativas = 0, variou = 0;
    int anterior = -1;
    for (int i = 0; i < PARTIDAS; i++) {
        anomalias_sortear(lista, 60);
        int mascara = 0;
        for (Anomalia *a = lista->inicio; a; a = a->prox) {
            ativas += a->ativa;
            mascara = mascara * 2 + a->ativa;
        }
        if (mascara != anterior) variou++;
        anterior = mascara;
    }
    double media = 100.0 * ativas / (PARTIDAS * (double)lista->total);
    printf("         média de anomalias ativas em %d partidas: %.1f%%\n", PARTIDAS, media);
    CONFERIR(media > 58.5 && media < 62.5, "cada anomalia entra em cerca de 60% das partidas");
    CONFERIR(variou > PARTIDAS / 2, "o conjunto de anomalias muda de uma partida para outra");

    a1->coletada = true;
    CONFERIR(anomalias_contar_coletadas(lista) == (a1->ativa ? 1 : 0), "contagem de coletadas considera só as ativas");
    anomalias_reiniciar(lista);
    CONFERIR(anomalias_contar_ativas(lista) == 0 && !a1->coletada, "reiniciar zera a lista para a próxima partida");

    CONFERIR(anomalias_remover(lista, a1) && lista->total == 3, "remoção de anomalia religou a lista");
    anomalias_liberar(lista);

    /* leitura do arquivo de anomalias */
    const char *caminho = "teste_anomalias.tmp";
    FILE *arquivo = fopen(caminho, "w");
    if (arquivo) {
        fputs("# comentário\nanomalia pasta_a Olhos de gente\nquadro 3 10 20 30 40\n"
              "area 50 60 70 80\nquadro 1 1 2 3 4\n\nanomalia pasta_b Sem cauda\nquadro 11 5 5 5 5\n", arquivo);
        fclose(arquivo);
        ListaAnomalias *lida = anomalias_criar();
        int n = anomalias_carregar(lida, caminho);
        Anomalia *pa = lida->inicio;
        CONFERIR(n == 2 && pa && strcmp(pa->pasta, "pasta_a") == 0 &&
                 strcmp(pa->descricao, "Olhos de gente") == 0 && pa->totalAparicoes == 2 &&
                 pa->aparicoes->quadro == 1 && pa->aparicoes->prox->totalAreas == 2 &&
                 pa->prox && pa->prox->aparicoes->quadro == 11,
                 "arquivo de anomalias lido (anomalias, quadros e áreas)");
        anomalias_liberar(lida);
        remove(caminho);
    }
}

static void testar_ranking(void)
{
    printf("\nRANKING (insertion sort em vetor)\n");

    Ranking ranking = { 0 };
    const float tempos[5] = { 95.0f, 42.5f, 120.0f, 42.5f, 61.0f };
    const int pausas[5] = { 1, 3, 0, 1, 2 };
    for (int i = 0; i < 5; i++) {
        Registro r = { 0 };
        snprintf(r.nome, sizeof r.nome, "TESTE%d", i);
        r.tempo = tempos[i];
        r.pausas = pausas[i];
        ranking_inserir(&ranking, r);
    }
    bool ordenado = true;
    for (int i = 1; i < ranking.total; i++)
        if (ranking.itens[i - 1].tempo > ranking.itens[i].tempo) ordenado = false;
    CONFERIR(ordenado, "menor tempo primeiro");
    CONFERIR(strcmp(ranking.itens[0].nome, "TESTE3") == 0, "no empate de tempo, vence quem pausou menos");

    for (int i = 0; i < 20; i++) {
        Registro r = { 0 };
        snprintf(r.nome, sizeof r.nome, "EXTRA%d", i);
        r.tempo = 200.0f + i;
        ranking_inserir(&ranking, r);
    }
    CONFERIR(ranking.total == RANKING_MAX, "o ranking guarda só os 10 melhores");
}

int main(void)
{
    srand(1927);
    testar_filme();
    testar_anomalias();
    testar_ranking();

    printf("\n%s\n", falhas == 0 ? "Todos os testes passaram." : "HÁ TESTES FALHANDO.");
    return falhas == 0 ? 0 : 1;
}
