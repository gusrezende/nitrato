/*
 * jogo.h — regras, telas e desenho do jogo.
 *
 * O jogo é uma máquina de estados (enum Tela). A cada volta do laço
 * principal, main.c lê o teclado e o mouse, monta uma struct Entrada e chama
 * jogo_atualizar() e jogo_desenhar().
 */
#ifndef JOGO_H
#define JOGO_H

#include "raylib.h"
#include "anomalia.h"
#include "filme.h"
#include "ranking.h"

/* ----------------------------------------------------------- configuração */

#define TITULO_JOGO        "NITRATO"
#define TITULO_FILME       "O Andarilho da Meia-Noite"
#define ANO_FILME          "1927"
#define NUMERO_CASO        "1127"

#define LARGURA_TELA       1280
#define ALTURA_TELA        720

#define FPS_FILME          11       /* quadros por segundo da animação          */
#define MAX_PAUSAS         3        /* a pausa seguinte a essa perde o jogo     */
#define PASSO_GASTA_PAUSA  1        /* 1: RETROCEDER/AVANÇAR com o filme rodando
                                       param o filme e gastam uma pausa;
                                       0: param o filme sem gastar              */
#define CHANCE_ANOMALIA    60       /* % de chance de cada anomalia estar
                                       escondida no filme em uma partida        */
#define PENALIDADE_ERRO    5.0f     /* segundos somados a cada clique errado    */
#define FOLGA_CLIQUE       8.0f     /* tolerância do clique, em pixels do frame */
#define TOLERANCIA_REACAO  0.20f    /* segundos em que o clique ainda vale depois
                                       de a anomalia sair da tela (0 = nenhum)  */

#define ARQUIVO_RANKING    "ranking.txt"

/* ------------------------------------------------------------------ tipos */

typedef enum Tela {
    TELA_MENU,
    TELA_COMO_JOGAR,
    TELA_RANKING,
    TELA_JOGO,
    TELA_FUGA,          /* todas as anomalias achadas: a entidade sai do filme */
    TELA_VITORIA,
    TELA_DERROTA,
    TELA_ERRO           /* faltam arquivos em assets/                          */
} Tela;

typedef enum EstadoProjetor {
    PROJETOR_PAUSA,     /* filme parado: só anda com RETROCEDER / AVANÇAR */
    PROJETOR_PLAY       /* filme rodando a FPS_FILME quadros por segundo  */
} EstadoProjetor;

/* Tudo o que o jogador fez neste instante (preenchido em main.c). */
typedef struct Entrada {
    float dt;           /* segundos desde a última atualização */
    Vector2 mouse;      /* posição na tela lógica de 1280x720   */
    bool clique;        /* botão esquerdo acabou de ser apertado */
    bool cima, baixo, esquerda, direita;
    bool confirmar;     /* ENTER     */
    bool espaco;        /* ESPAÇO    */
    bool voltar;        /* ESC       */
    bool apagar;        /* BACKSPACE */
    int caractere;      /* letra digitada (0 = nenhuma) */
} Entrada;

typedef struct Jogo {
    Tela tela;
    bool sair;
    float tempoTela;            /* segundos desde que a tela atual abriu */
    int opcao;                  /* item destacado nos menus              */
    Vector2 mouse, mouseAnterior;
    char erro[256];

    /* estruturas de dados */
    Filme *filme;               /* lista duplamente encadeada circular   */
    ListaAnomalias *anomalias;  /* lista simplesmente encadeada          */
    Ranking ranking;

    /* recursos */
    Font fonteTitulo, fonteUI, fonteMaquina;
    bool fontesProprias;
    Texture2D entidade;         /* assets/entidade.png (opcional)        */
    bool temEntidade;
    Texture2D granulado;        /* ruído do filtro de filme antigo       */
    Rectangle areaFilme;        /* onde o frame é desenhado na tela      */
    float larguraRef, alturaRef;/* tamanho original do frame, em pixels  */

    /* partida em andamento */
    EstadoProjetor projetor;
    float acumulador;           /* fração de quadro ainda não exibida    */
    double tempo;               /* segundos de investigação              */
    int pausasUsadas;
    int erros;
    int totalAtivas;            /* anomalias sorteadas para esta partida */
    int coletadas;
    int sessao;                 /* em que volta do rolo o filme está     */
    Anomalia *vistaAnomalia;    /* última anomalia que passou na tela    */
    Aparicao *vistaAparicao;
    float tempoDesdeVista;      /* há quanto tempo ela saiu da tela      */
    unsigned tique;             /* avança junto com o filme (efeitos)    */

    /* retorno visual */
    char aviso[64];
    float avisoTempo;
    bool avisoBom;
    Vector2 marcadorPos;
    float marcadorTempo;
    bool marcadorBom;
    float tremor;
    Vector2 origemFuga;

    /* fim de jogo */
    char nome[NOME_MAX + 1];
    int posicaoRanking;         /* linha a destacar no ranking (-1 = nenhuma) */
} Jogo;

/* ---------------------------------------------------- funções principais */

/* Carrega fontes, frames, anomalias e ranking. Chamar depois de InitWindow(). */
void jogo_iniciar(Jogo *jogo, const char *pastaAssets);

/* Começa uma investigação: sorteia as anomalias da partida e esconde cada
 * uma em um quadro do filme. */
void jogo_nova_partida(Jogo *jogo);

/* Aplica as regras para um instante de jogo. */
void jogo_atualizar(Jogo *jogo, const Entrada *entrada);

/* Desenha a tela atual (entre BeginDrawing/BeginTextureMode e o End). */
void jogo_desenhar(const Jogo *jogo);

/* Tempo final da partida, já com as penalidades por clique errado. */
float jogo_tempo_final(const Jogo *jogo);

/* Libera texturas, fontes e as listas. */
void jogo_encerrar(Jogo *jogo);

#endif
