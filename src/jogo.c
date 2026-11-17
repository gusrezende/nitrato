#include "jogo.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ cores */

#define COR_FUNDO    (Color){  12,  10,   8, 255 }
#define COR_PAINEL   (Color){  24,  21,  17, 255 }
#define COR_RELEVO   (Color){  52,  45,  36, 255 }
#define COR_CREME    (Color){ 232, 220, 192, 255 }
#define COR_OURO     (Color){ 201, 164,  92, 255 }
#define COR_APAGADO  (Color){ 136, 124, 102, 255 }
#define COR_SANGUE   (Color){ 190,  52,  38, 255 }

/* ----------------------------------------------------------------- layout */

static const Rectangle VAO_FILME = { 40, 44, 800, 600 };    /* "tela de cinema" */
static const Rectangle LINHA_TEMPO = { 40, 652, 800, 6 };
static const Rectangle PAINEL = { 868, 44, 372, 664 };

#define MENU_Y          372.0f
#define DURACAO_FUGA    4.6f
#define DURACAO_QUEIMA  2.8f
#define MAX_EVIDENCIAS  8       /* linhas do dossiê que cabem no painel */

enum { BOTAO_RETROCEDER, BOTAO_PAUSAR, BOTAO_PLAY, BOTAO_AVANCAR };

static Rectangle rect_botao(int i)
{
    return (Rectangle){ 40.0f + i * 204.0f, 668.0f, 188.0f, 40.0f };
}

static Rectangle rect_opcao(int i, float y0)
{
    return (Rectangle){ LARGURA_TELA / 2.0f - 230.0f, y0 + i * 56.0f, 460.0f, 46.0f };
}

/* ------------------------------------------------------------------ texto */

typedef enum { FONTE_TITULO, FONTE_UI, FONTE_MAQUINA } TipoFonte;

static Font fonte_de(const Jogo *j, TipoFonte tipo)
{
    if (tipo == FONTE_TITULO) return j->fonteTitulo;
    if (tipo == FONTE_UI) return j->fonteUI;
    return j->fonteMaquina;
}

static float espacamento(const Jogo *j, TipoFonte tipo, float tamanho)
{
    if (!j->fontesProprias) return tamanho / 10.0f;     /* fonte padrão da raylib */
    return (tipo == FONTE_MAQUINA) ? 0.0f : tamanho * 0.04f;
}

static float medir(const Jogo *j, TipoFonte tipo, const char *texto, float tamanho)
{
    return MeasureTextEx(fonte_de(j, tipo), texto, tamanho, espacamento(j, tipo, tamanho)).x;
}

static void escrever(const Jogo *j, TipoFonte tipo, const char *texto,
                     float x, float y, float tamanho, Color cor)
{
    DrawTextEx(fonte_de(j, tipo), texto, (Vector2){ roundf(x), roundf(y) },
               tamanho, espacamento(j, tipo, tamanho), cor);
}

static void escrever_centro(const Jogo *j, TipoFonte tipo, const char *texto,
                            float centroX, float y, float tamanho, Color cor)
{
    escrever(j, tipo, texto, centroX - medir(j, tipo, texto, tamanho) / 2.0f, y, tamanho, cor);
}

static void escrever_direita(const Jogo *j, TipoFonte tipo, const char *texto,
                             float direitaX, float y, float tamanho, Color cor)
{
    escrever(j, tipo, texto, direitaX - medir(j, tipo, texto, tamanho), y, tamanho, cor);
}

/* Copia 'texto' para 'saida', cortando com reticências se passar da largura. */
static void encaixar(const Jogo *j, TipoFonte tipo, const char *texto, float tamanho,
                     float larguraMax, char *saida, size_t capacidade)
{
    snprintf(saida, capacidade, "%s", texto);
    if (medir(j, tipo, saida, tamanho) <= larguraMax) return;

    size_t n = strlen(saida);
    while (n > 0) {
        /* apaga o último caractere inteiro (em UTF-8 ele pode ter vários bytes) */
        do { n--; } while (n > 0 && ((unsigned char)saida[n] & 0xC0) == 0x80);
        if (n + 4 > capacidade) continue;
        snprintf(saida + n, capacidade - n, "...");
        if (medir(j, tipo, saida, tamanho) <= larguraMax) return;
    }
}

static const char *relogio(double segundos)
{
    int total = (int)segundos;
    return TextFormat("%02d:%02d", total / 60, total % 60);
}

/* Gerador pseudoaleatório simples para os efeitos de filme velho. Usar uma
 * semente própria (em vez de rand) faz o desenho ser sempre o mesmo para o
 * mesmo quadro: a sujeira não "ferve" enquanto o filme está pausado. */
static unsigned sorteio(unsigned *semente)
{
    unsigned x = *semente ? *semente : 0x9E3779B9u;     /* xorshift não aceita zero */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *semente = x;
    return x;
}

/* ----------------------------------------------------------- carregamento */

static Font carregar_fonte(const char *caminho, int tamanho, bool *todasOk)
{
    int pontos[240];
    int n = 0;
    for (int c = 32; c <= 126; c++) pontos[n++] = c;    /* letras, números, sinais */
    for (int c = 161; c <= 255; c++) pontos[n++] = c;   /* letras acentuadas        */
    pontos[n++] = 0x2013;   /* meia-risca  */
    pontos[n++] = 0x2014;   /* travessão   */
    pontos[n++] = 0x201C;   /* aspas curvas */
    pontos[n++] = 0x201D;
    pontos[n++] = 0x2026;   /* reticências */

    if (FileExists(caminho)) {
        Font fonte = LoadFontEx(caminho, tamanho, pontos, n);
        if (fonte.texture.id != 0) {
            SetTextureFilter(fonte.texture, TEXTURE_FILTER_BILINEAR);
            return fonte;
        }
    }
    *todasOk = false;
    return GetFontDefault();
}

/* frame_0042.png -> 42. Devolve -1 se o nome não tiver número. */
static int numero_do_arquivo(const char *caminho)
{
    const char *nome = GetFileNameWithoutExt(caminho);
    int fim = (int)strlen(nome) - 1;
    while (fim >= 0 && !isdigit((unsigned char)nome[fim])) fim--;
    if (fim < 0) return -1;
    int inicio = fim;
    while (inicio > 0 && isdigit((unsigned char)nome[inicio - 1])) inicio--;
    return atoi(nome + inicio);
}

/* ---------------------------------------------- filtro de filme antigo */

/*
 * Carrega um frame e aplica a parte "química" do filtro de filme antigo:
 * a imagem perde as cores, ganha contraste e é revelada em sépia (preto
 * quente nas sombras, papel amarelado nas luzes). A parte "mecânica" do
 * filtro — granulado, riscos, poeira, cintilação — é desenhada por cima a
 * cada quadro, em desenhar_quadro().
 *
 * O filtro vale para qualquer imagem que entrar em assets/, inclusive as
 * versões com anomalia: basta salvar os frames com as cores originais.
 *
 * 'largura' e 'altura' recebem o tamanho ORIGINAL da imagem; é nessa medida
 * que as áreas clicáveis de anomalias.txt são escritas.
 */
static Texture2D carregar_frame_antigo(const char *caminho, int *largura, int *altura)
{
    static const Color SOMBRA = { 26, 21, 15, 255 };
    static const Color LUZ = { 224, 211, 182, 255 };
    Texture2D vazia = { 0 };

    Image imagem = LoadImage(caminho);
    if (imagem.data == NULL) return vazia;
    if (largura) *largura = imagem.width;
    if (altura) *altura = imagem.height;

    /* reduz para o tamanho em que o frame aparece na tela: economiza memória
     * de vídeo e deixa os traços finos mais limpos do que reduzir na hora */
    float escala = fminf(VAO_FILME.width / imagem.width, VAO_FILME.height / imagem.height);
    if (escala < 1.0f)
        ImageResize(&imagem, (int)(imagem.width * escala + 0.5f), (int)(imagem.height * escala + 0.5f));

    Color *pixels = LoadImageColors(imagem);
    int total = imagem.width * imagem.height;
    for (int i = 0; i < total; i++) {
        /* 1. preto e branco: brilho percebido pelo olho */
        float tom = (0.299f * pixels[i].r + 0.587f * pixels[i].g + 0.114f * pixels[i].b) / 255.0f;

        /* 2. contraste duro de película ortocromática: os meios-tons escurecem
         *    e as luzes estouram um pouco */
        tom = powf(tom, 1.7f) * 1.08f;
        if (tom > 1.0f) tom = 1.0f;

        /* 3. viragem em sépia */
        pixels[i].r = (unsigned char)(SOMBRA.r + (LUZ.r - SOMBRA.r) * tom);
        pixels[i].g = (unsigned char)(SOMBRA.g + (LUZ.g - SOMBRA.g) * tom);
        pixels[i].b = (unsigned char)(SOMBRA.b + (LUZ.b - SOMBRA.b) * tom);
        pixels[i].a = 255;
    }

    Image revelada = { pixels, imagem.width, imagem.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D textura = LoadTextureFromImage(revelada);
    UnloadImageColors(pixels);
    UnloadImage(imagem);

    if (textura.id != 0) SetTextureFilter(textura, TEXTURE_FILTER_BILINEAR);
    return textura;
}

/* Cria a textura de granulado: pontos claros e escuros quase transparentes,
 * espalhados ao acaso, que são sorteados de novo a cada quadro do filme. */
static Texture2D criar_granulado(void)
{
    enum { LADO = 256 };
    Texture2D vazia = { 0 };
    Color *pixels = malloc(LADO * LADO * sizeof *pixels);
    if (!pixels) return vazia;

    unsigned semente = 1927u;
    for (int i = 0; i < LADO * LADO; i++) {
        unsigned char tom = (sorteio(&semente) % 2) ? 255 : 0;
        pixels[i] = (Color){ tom, tom, tom, (unsigned char)(sorteio(&semente) % 44) };
    }

    Image imagem = { pixels, LADO, LADO, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D textura = LoadTextureFromImage(imagem);
    free(pixels);

    if (textura.id != 0) {
        SetTextureFilter(textura, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(textura, TEXTURE_WRAP_REPEAT);   /* repete para cobrir a tela */
    }
    return textura;
}

/* Lê assets/frames/ e monta o rolo. Os arquivos vêm do sistema em ordem
 * qualquer, por isso o rolo é ordenado (merge sort) no final. */
static void carregar_frames(Jogo *j, const char *pastaAssets)
{
    char pasta[512];
    snprintf(pasta, sizeof pasta, "%s/frames", pastaAssets);
    if (!DirectoryExists(pasta)) return;

    FilePathList arquivos = LoadDirectoryFilesEx(pasta, ".png", false);
    for (unsigned i = 0; i < arquivos.count; i++) {
        int numero = numero_do_arquivo(arquivos.paths[i]);
        if (numero < 0 || filme_buscar(j->filme, numero)) {
            TraceLog(LOG_WARNING, "FILME: arquivo ignorado (sem número ou repetido): %s",
                     arquivos.paths[i]);
            continue;
        }
        int largura = 0, altura = 0;
        Texture2D imagem = carregar_frame_antigo(arquivos.paths[i], &largura, &altura);
        if (imagem.id == 0) continue;
        if (j->filme->total == 0) {             /* o primeiro frame define a medida */
            j->larguraRef = (float)largura;
            j->alturaRef = (float)altura;
        }
        filme_inserir(j->filme, numero, imagem);
    }
    UnloadDirectoryFiles(arquivos);

    filme_ordenar(j->filme);
}


/* Lê assets/anomalias.txt e depois as imagens de cada anomalia, em
 * assets/anomalias/<pasta>/. Aparições sem imagem, ou que apontam para um
 * quadro que o filme não tem, são descartadas; anomalias que ficarem sem
 * nenhuma aparição saem da lista. */
static void carregar_anomalias(Jogo *j, const char *pastaAssets)
{
    char caminho[512];
    snprintf(caminho, sizeof caminho, "%s/anomalias.txt", pastaAssets);
    anomalias_carregar(j->anomalias, caminho);

    Anomalia *a = j->anomalias->inicio;
    while (a) {
        Anomalia *seguinte = a->prox;

        /* 1. cada arquivo da pasta vira a imagem da aparição de mesmo número */
        snprintf(caminho, sizeof caminho, "%s/anomalias/%s", pastaAssets, a->pasta);
        if (DirectoryExists(caminho)) {
            FilePathList arquivos = LoadDirectoryFilesEx(caminho, ".png", false);
            for (unsigned i = 0; i < arquivos.count; i++) {
                int numero = numero_do_arquivo(arquivos.paths[i]);
                Aparicao *ap = anomalia_buscar_aparicao(a, numero);
                if (!ap || ap->imagem.id != 0 || !filme_buscar(j->filme, numero)) continue;
                ap->imagem = carregar_frame_antigo(arquivos.paths[i], NULL, NULL);
            }
            UnloadDirectoryFiles(arquivos);
        }

        /* 2. poda o que ficou sem imagem */
        Aparicao *ap = a->aparicoes;
        while (ap) {
            Aparicao *proxima = ap->prox;
            if (ap->imagem.id == 0) {
                TraceLog(LOG_WARNING, "ANOMALIA: %s, quadro %d: sem imagem ou sem quadro no filme",
                         a->pasta, ap->quadro);
                anomalia_remover_aparicao(a, ap);
            }
            ap = proxima;
        }
        if (a->totalAparicoes == 0) anomalias_remover(j->anomalias, a);

        a = seguinte;
    }
}

void jogo_iniciar(Jogo *j, const char *pastaAssets)
{
    memset(j, 0, sizeof *j);
    srand((unsigned)time(NULL));

    j->fontesProprias = true;
    j->fonteTitulo  = carregar_fonte(TextFormat("%s/fontes/Limelight-Regular.ttf", pastaAssets), 96, &j->fontesProprias);
    j->fonteUI      = carregar_fonte(TextFormat("%s/fontes/Limelight-Regular.ttf", pastaAssets), 30, &j->fontesProprias);
    j->fonteMaquina = carregar_fonte(TextFormat("%s/fontes/SpecialElite-Regular.ttf", pastaAssets), 22, &j->fontesProprias);

    j->granulado = criar_granulado();
    j->filme = filme_criar();
    j->anomalias = anomalias_criar();
    carregar_frames(j, pastaAssets);
    carregar_anomalias(j, pastaAssets);

    const char *entidade = TextFormat("%s/entidade.png", pastaAssets);
    if (FileExists(entidade)) {
        j->entidade = LoadTexture(entidade);
        j->temEntidade = (j->entidade.id != 0);
        if (j->temEntidade) SetTextureFilter(j->entidade, TEXTURE_FILTER_BILINEAR);
    }

    ranking_carregar(&j->ranking, ARQUIVO_RANKING);
    j->posicaoRanking = -1;
    j->areaFilme = VAO_FILME;
    j->tela = TELA_MENU;

    if (j->filme->total == 0) {
        snprintf(j->erro, sizeof j->erro,
                 "Nenhum frame encontrado em %s/frames/ (use arquivos .png numerados).", pastaAssets);
    } else if (j->anomalias->total == 0) {
        snprintf(j->erro, sizeof j->erro,
                 "Nenhuma anomalia com imagem: confira %s/anomalias.txt e %s/anomalias/.",
                 pastaAssets, pastaAssets);
    }
    if (j->erro[0]) {
        j->tela = TELA_ERRO;
        return;
    }

    /* o frame é encaixado no vão de 800x600 sem deformar */
    float escala = fminf(VAO_FILME.width / j->larguraRef, VAO_FILME.height / j->alturaRef);
    j->areaFilme.width = j->larguraRef * escala;
    j->areaFilme.height = j->alturaRef * escala;
    j->areaFilme.x = VAO_FILME.x + (VAO_FILME.width - j->areaFilme.width) / 2.0f;
    j->areaFilme.y = VAO_FILME.y + (VAO_FILME.height - j->areaFilme.height) / 2.0f;
}

static void descarregar_quadro(Quadro *quadro, void *dados)
{
    (void)dados;
    UnloadTexture(quadro->imagem);
}

void jogo_encerrar(Jogo *j)
{
    filme_percorrer(j->filme, descarregar_quadro, NULL);
    filme_liberar(j->filme);
    for (Anomalia *a = j->anomalias->inicio; a; a = a->prox)
        for (Aparicao *ap = a->aparicoes; ap; ap = ap->prox)
            UnloadTexture(ap->imagem);
    anomalias_liberar(j->anomalias);
    j->filme = NULL;
    j->anomalias = NULL;

    if (j->temEntidade) UnloadTexture(j->entidade);
    if (j->granulado.id != 0) UnloadTexture(j->granulado);

    unsigned padrao = GetFontDefault().texture.id;
    if (j->fonteTitulo.texture.id != padrao) UnloadFont(j->fonteTitulo);
    if (j->fonteUI.texture.id != padrao) UnloadFont(j->fonteUI);
    if (j->fonteMaquina.texture.id != padrao) UnloadFont(j->fonteMaquina);
}

/* ----------------------------------------------------------------- regras */

static void mudar_tela(Jogo *j, Tela tela)
{
    j->tela = tela;
    j->tempoTela = 0.0f;
    j->opcao = 0;
}

static void avisar(Jogo *j, const char *texto, bool bom)
{
    snprintf(j->aviso, sizeof j->aviso, "%s", texto);
    j->avisoTempo = 1.6f;
    j->avisoBom = bom;
}

float jogo_tempo_final(const Jogo *j)
{
    return (float)j->tempo + j->erros * PENALIDADE_ERRO;
}


/*
 * Começa uma partida. As anomalias são definidas aqui e não mudam mais até
 * a próxima partida:
 *
 *   1. sorteio: cada anomalia tem 60% de chance (CHANCE_ANOMALIA) de estar
 *      escondida no filme desta vez;
 *   2. ordenação: as sorteadas vão para a frente da lista, começando pelas
 *      que têm menos quadros possíveis;
 *   3. nessa ordem, cada uma escolhe ao acaso um quadro ainda livre e fica
 *      plantada nele. Como as mais limitadas escolhem primeiro, nenhuma
 *      fica sem lugar por culpa de outra.
 *
 * Cada anomalia altera, portanto, um único quadro do filme, sempre o mesmo,
 * até ser registrada.
 */
void jogo_nova_partida(Jogo *j)
{
    if (j->erro[0]) return;

    filme_remover_anomalia(j->filme, NULL);
    anomalias_reiniciar(j->anomalias);

    anomalias_sortear(j->anomalias, CHANCE_ANOMALIA);
    anomalias_ordenar(j->anomalias);
    for (Anomalia *a = j->anomalias->inicio; a && a->ativa; a = a->prox)
        if (!filme_plantar_em_quadro_livre(j->filme, a)) a->ativa = false;

    j->totalAtivas = anomalias_contar_ativas(j->anomalias);
    j->coletadas = 0;
    j->pausasUsadas = 0;
    j->erros = 0;
    j->tempo = 0.0;
    j->sessao = 1;
    j->acumulador = 0.0f;
    j->projetor = PROJETOR_PLAY;
    j->avisoTempo = 0.0f;
    j->marcadorTempo = 0.0f;
    j->tremor = 0.0f;
    j->vistaAnomalia = NULL;
    j->vistaAparicao = NULL;
    j->tempoDesdeVista = 0.0f;
    j->nome[0] = '\0';
    filme_rebobinar(j->filme);

    mudar_tela(j, TELA_JOGO);
    avisar(j, "A SESSÃO COMEÇOU", true);
}

/* Para ou solta o filme. É aqui que o limite de pausas é cobrado: toda vez
 * que o filme rodando é parado, uma pausa é gasta. */
static void acionar_projetor(Jogo *j, EstadoProjetor novo, bool gastaPausa)
{
    if (novo == j->projetor) return;

    if (novo == PROJETOR_PAUSA && gastaPausa) {
        j->pausasUsadas++;
        if (j->pausasUsadas > MAX_PAUSAS) {
            j->projetor = PROJETOR_PAUSA;
            mudar_tela(j, TELA_DERROTA);
            return;
        }
        avisar(j, TextFormat("PAUSA %d DE %d", j->pausasUsadas, MAX_PAUSAS),
               j->pausasUsadas < MAX_PAUSAS);
    }
    j->projetor = novo;
    j->acumulador = 0.0f;
}

/* RETROCEDER (-1) e AVANÇAR (+1): o projetor anda exatamente um quadro por
 * clique. Se o filme estava rodando, ele para antes de dar o passo. */
static void passo_do_projetor(Jogo *j, int direcao)
{
    acionar_projetor(j, PROJETOR_PAUSA, PASSO_GASTA_PAUSA);
    if (j->tela != TELA_JOGO) return;           /* parar o filme acabou de queimá-lo */

    if (direcao < 0) {
        filme_retroceder(j->filme);
    } else if (filme_avancar(j->filme) == j->filme->inicio) {
        j->sessao++;
    }
    j->tique++;
}

/* Faz o rolo andar sozinho: o projetor é um ponteiro caminhando pela lista. */
static void rodar_filme(Jogo *j, float dt)
{
    if (j->projetor != PROJETOR_PLAY) return;

    j->acumulador += dt * FPS_FILME;
    while (j->acumulador >= 1.0f) {
        j->acumulador -= 1.0f;
        j->tique++;
        if (filme_avancar(j->filme) == j->filme->inicio) j->sessao++;   /* o rolo deu a volta */
    }
}

/* O jogador clicou na tela de cinema: havia uma anomalia ali? */
static void investigar(Jogo *j, Vector2 mouse)
{
    Quadro *quadro = j->filme->atual;
    Anomalia *anomalia = quadro->anomalia;
    Aparicao *aparicao = quadro->aparicao;
    int numero = quadro->numero;

    /* a anomalia dura um único quadro (1/11 de segundo); o clique ainda vale
     * por um instante depois que ela sai da tela */
    if (!anomalia && j->vistaAnomalia && !j->vistaAnomalia->coletada &&
        j->tempoDesdeVista <= TOLERANCIA_REACAO) {
        anomalia = j->vistaAnomalia;
        aparicao = j->vistaAparicao;
        numero = aparicao->quadro;
    }

    /* converte a posição do mouse para pixels da imagem do frame */
    float px = (mouse.x - j->areaFilme.x) / j->areaFilme.width * j->larguraRef;
    float py = (mouse.y - j->areaFilme.y) / j->areaFilme.height * j->alturaRef;

    j->marcadorPos = mouse;
    j->marcadorTempo = 0.6f;

    if (anomalia && aparicao_contem_ponto(aparicao, px, py, FOLGA_CLIQUE)) {
        j->coletadas++;
        anomalia->coletada = true;
        anomalia->ordemColeta = j->coletadas;
        anomalia->quadroColeta = numero;
        anomalia->tempoColeta = j->tempo;
        filme_remover_anomalia(j->filme, anomalia);     /* o filme "sara" */
        j->vistaAnomalia = NULL;
        j->vistaAparicao = NULL;
        j->marcadorBom = true;
        avisar(j, "EVIDÊNCIA REGISTRADA", true);
        acionar_projetor(j, PROJETOR_PLAY, false);   /* achou a anomalia: o filme volta a rodar */

        if (j->coletadas >= j->totalAtivas) {
            j->origemFuga = mouse;
            mudar_tela(j, TELA_FUGA);
        }
    } else {
        j->erros++;
        j->marcadorBom = false;
        j->tremor = 0.3f;
        avisar(j, TextFormat("NADA AQUI  +%d s", (int)PENALIDADE_ERRO), false);
    }
}

/* Navegação comum a todos os menus. Devolve a opção confirmada, ou -1. */
static int escolher_opcao(Jogo *j, const Entrada *e, int total, float y0)
{
    if (e->baixo) j->opcao = (j->opcao + 1) % total;
    if (e->cima) j->opcao = (j->opcao + total - 1) % total;

    bool mouseMexeu = (e->mouse.x != j->mouseAnterior.x || e->mouse.y != j->mouseAnterior.y);
    for (int i = 0; i < total; i++) {
        if (!CheckCollisionPointRec(e->mouse, rect_opcao(i, y0))) continue;
        if (mouseMexeu) j->opcao = i;
        if (e->clique) return i;
    }
    return e->confirmar ? j->opcao : -1;
}

static void atualizar_partida(Jogo *j, const Entrada *e, float dt)
{
    j->tempo += dt;
    if (j->avisoTempo > 0.0f) j->avisoTempo -= dt;
    if (j->marcadorTempo > 0.0f) j->marcadorTempo -= dt;
    if (j->tremor > 0.0f) j->tremor -= dt;

    if (e->voltar) {
        mudar_tela(j, TELA_MENU);
        return;
    }

    /* botões do projetor, da esquerda para a direita */
    int botao = -1;
    for (int i = 0; i < 4; i++)
        if (e->clique && CheckCollisionPointRec(e->mouse, rect_botao(i))) botao = i;

    if (botao == BOTAO_RETROCEDER || e->esquerda) passo_do_projetor(j, -1);
    if (botao == BOTAO_AVANCAR || e->direita) passo_do_projetor(j, +1);
    if (botao == BOTAO_PAUSAR) acionar_projetor(j, PROJETOR_PAUSA, true);
    if (botao == BOTAO_PLAY) acionar_projetor(j, PROJETOR_PLAY, true);
    if (e->espaco)
        acionar_projetor(j, j->projetor == PROJETOR_PAUSA ? PROJETOR_PLAY : PROJETOR_PAUSA, true);
    if (j->tela != TELA_JOGO) return;           /* a 4ª pausa acabou de queimar o filme */

    rodar_filme(j, dt);

    /* lembra a última anomalia que passou pela tela e há quanto tempo */
    if (j->filme->atual->anomalia) {
        j->vistaAnomalia = j->filme->atual->anomalia;
        j->vistaAparicao = j->filme->atual->aparicao;
        j->tempoDesdeVista = 0.0f;
    } else {
        j->tempoDesdeVista += dt;
    }

    if (e->clique && CheckCollisionPointRec(e->mouse, j->areaFilme))
        investigar(j, e->mouse);
}

static void atualizar_vitoria(Jogo *j, const Entrada *e)
{
    size_t n = strlen(j->nome);
    int c = e->caractere;
    if (c >= 32 && c <= 126 && c != ';' && n < NOME_MAX && !(c == ' ' && n == 0)) {
        j->nome[n] = (char)toupper(c);
        j->nome[n + 1] = '\0';
    }
    if (e->apagar && n > 0) j->nome[n - 1] = '\0';

    if (escolher_opcao(j, e, 1, 560.0f) == 0) {
        Registro registro;
        memset(&registro, 0, sizeof registro);
        snprintf(registro.nome, sizeof registro.nome, "%s", j->nome[0] ? j->nome : "ANONIMO");
        registro.tempo = jogo_tempo_final(j);
        registro.pausas = j->pausasUsadas;
        registro.erros = j->erros;

        j->posicaoRanking = ranking_inserir(&j->ranking, registro);
        ranking_salvar(&j->ranking, ARQUIVO_RANKING);
        mudar_tela(j, TELA_RANKING);
    }
}

void jogo_atualizar(Jogo *j, const Entrada *e)
{
    float dt = (e->dt > 0.1f) ? 0.1f : e->dt;   /* evita saltos se a janela travar */
    j->tempoTela += dt;
    j->mouse = e->mouse;

    switch (j->tela) {
    case TELA_MENU:
        switch (escolher_opcao(j, e, 4, MENU_Y)) {
        case 0: jogo_nova_partida(j); break;
        case 1: mudar_tela(j, TELA_COMO_JOGAR); break;
        case 2: j->posicaoRanking = -1; mudar_tela(j, TELA_RANKING); break;
        case 3: j->sair = true; break;
        }
        break;

    case TELA_COMO_JOGAR:
    case TELA_RANKING:
        if (e->voltar || escolher_opcao(j, e, 1, 600.0f) == 0) mudar_tela(j, TELA_MENU);
        break;

    case TELA_JOGO:
        atualizar_partida(j, e, dt);
        break;

    case TELA_FUGA:
        if (j->tempoTela >= DURACAO_FUGA ||
            (j->tempoTela > 1.0f && (e->clique || e->confirmar)))
            mudar_tela(j, TELA_VITORIA);
        break;

    case TELA_VITORIA:
        atualizar_vitoria(j, e);
        break;

    case TELA_DERROTA:
        if (j->tempoTela < DURACAO_QUEIMA) break;
        switch (escolher_opcao(j, e, 2, 480.0f)) {
        case 0: jogo_nova_partida(j); break;
        case 1: mudar_tela(j, TELA_MENU); break;
        }
        break;

    case TELA_ERRO:
        if (e->voltar || escolher_opcao(j, e, 1, 520.0f) == 0) j->sair = true;
        break;
    }

    j->mouseAnterior = e->mouse;
}

/* ================================================================ DESENHO */

static void seta_direita(float x, float centroY, float largura, float meiaAltura, Color cor)
{
    DrawTriangle((Vector2){ x, centroY - meiaAltura }, (Vector2){ x, centroY + meiaAltura },
                 (Vector2){ x + largura, centroY }, cor);
}

static void seta_esquerda(float x, float centroY, float largura, float meiaAltura, Color cor)
{
    DrawTriangle((Vector2){ x + largura, centroY - meiaAltura }, (Vector2){ x, centroY },
                 (Vector2){ x + largura, centroY + meiaAltura }, cor);
}

static void losango(float x, float y, float raio, Color cor)
{
    DrawTriangle((Vector2){ x, y - raio }, (Vector2){ x - raio, y }, (Vector2){ x + raio, y }, cor);
    DrawTriangle((Vector2){ x - raio, y }, (Vector2){ x, y + raio }, (Vector2){ x + raio, y }, cor);
}

/* Fundo das telas de texto: tiras de película nas laterais e moldura dupla,
 * no estilo dos intertítulos do cinema mudo. */
static void desenhar_moldura(const Jogo *j)
{
    ClearBackground(COR_FUNDO);

    float rolagem = fmodf(j->tempoTela * 28.0f, 56.0f);
    for (int lado = 0; lado < 2; lado++) {
        float x = lado ? LARGURA_TELA - 52.0f : 0.0f;
        DrawRectangle((int)x, 0, 52, ALTURA_TELA, COR_PAINEL);
        for (int i = -1; i < 14; i++) {         /* perfurações rolando */
            Rectangle furo = { x + 15.0f, i * 56.0f + rolagem + 10.0f, 22.0f, 32.0f };
            DrawRectangleRounded(furo, 0.35f, 6, COR_FUNDO);
        }
        DrawRectangle((int)(lado ? x : x + 51.0f), 0, 1, ALTURA_TELA, COR_RELEVO);
    }

    DrawRectangleLinesEx((Rectangle){ 84, 32, 1112, 656 }, 2.0f, COR_OURO);
    DrawRectangleLinesEx((Rectangle){ 94, 42, 1092, 636 }, 1.0f, Fade(COR_OURO, 0.45f));
    losango(94, 42, 7, COR_OURO);
    losango(1186, 42, 7, COR_OURO);
    losango(94, 678, 7, COR_OURO);
    losango(1186, 678, 7, COR_OURO);
}

static void desenhar_ornamento(float y)
{
    float cx = LARGURA_TELA / 2.0f;
    DrawRectangle((int)cx - 190, (int)y, 170, 1, COR_OURO);
    DrawRectangle((int)cx + 20, (int)y, 170, 1, COR_OURO);
    losango(cx, y, 6, COR_OURO);
}

static void desenhar_opcoes(const Jogo *j, const char *const *opcoes, int total, float y0)
{
    for (int i = 0; i < total; i++) {
        Rectangle r = rect_opcao(i, y0);
        bool escolhida = (i == j->opcao);
        float largura = medir(j, FONTE_UI, opcoes[i], 30);
        float centroX = r.x + r.width / 2.0f, centroY = r.y + r.height / 2.0f;

        escrever_centro(j, FONTE_UI, opcoes[i], centroX, r.y + 8.0f, 30,
                        escolhida ? COR_OURO : COR_CREME);
        if (escolhida) {
            seta_direita(centroX - largura / 2.0f - 30.0f, centroY, 12, 8, COR_OURO);
            seta_esquerda(centroX + largura / 2.0f + 18.0f, centroY, 12, 8, COR_OURO);
        }
    }
}

/* ------------------------------------------------------- tela de cinema */

/* Desenha o quadro atual do rolo com o desgaste de um filme de 1927. */
static void desenhar_quadro(const Jogo *j, float deslocX, float deslocY)
{
    const Quadro *quadro = j->filme->atual;
    Rectangle a = j->areaFilme;
    unsigned semente = j->tique * 2654435761u + 977u;
    sorteio(&semente);
    sorteio(&semente);

    DrawRectangleRec(VAO_FILME, BLACK);

    /* a película balança um pouco na janela do projetor */
    deslocY += (float)(sorteio(&semente) % 3) - 1.0f;

    /* é aqui que a anomalia aparece: se há uma aparição plantada neste
     * quadro, entra a versão alterada do frame no lugar da original */
    Texture2D imagem = quadro->aparicao ? quadro->aparicao->imagem : quadro->imagem;
    DrawTexturePro(imagem, (Rectangle){ 0, 0, (float)imagem.width, (float)imagem.height },
                   (Rectangle){ a.x + deslocX, a.y + deslocY, a.width, a.height },
                   (Vector2){ 0, 0 }, 0.0f, WHITE);

    /* se o frame não ocupa o vão inteiro (ex.: imagem 16:9 em vão 4:3), as
     * tarjas pretas escondem o que o balanço da película deixaria vazar */
    DrawRectangleRec((Rectangle){ VAO_FILME.x, VAO_FILME.y, VAO_FILME.width, a.y - VAO_FILME.y }, BLACK);
    DrawRectangleRec((Rectangle){ VAO_FILME.x, a.y + a.height, VAO_FILME.width,
                                  VAO_FILME.y + VAO_FILME.height - a.y - a.height }, BLACK);
    DrawRectangleRec((Rectangle){ VAO_FILME.x, VAO_FILME.y, a.x - VAO_FILME.x, VAO_FILME.height }, BLACK);
    DrawRectangleRec((Rectangle){ a.x + a.width, VAO_FILME.y,
                                  VAO_FILME.x + VAO_FILME.width - a.x - a.width, VAO_FILME.height }, BLACK);

    /* granulado: o mesmo ruído, deslocado ao acaso a cada quadro do filme */
    if (j->granulado.id != 0) {
        Rectangle trecho = { (float)(sorteio(&semente) % 256), (float)(sorteio(&semente) % 256),
                             a.width / 1.5f, a.height / 1.5f };
        DrawTexturePro(j->granulado, trecho, a, (Vector2){ 0, 0 }, 0.0f, WHITE);
    }

    /* cintilação da lâmpada */
    DrawRectangleRec(a, Fade(BLACK, (sorteio(&semente) % 100) / 100.0f * 0.08f));

    /* de vez em quando, uma mancha na emulsão — só perto das bordas laterais,
     * para não ser confundida com uma anomalia no meio do desenho */
    if (sorteio(&semente) % 100 < 22) {
        float faixa = a.width * 0.10f;
        float x = a.x + 20 + sorteio(&semente) % (unsigned)faixa;
        if (sorteio(&semente) % 2) x = a.x + a.width - 20 - sorteio(&semente) % (unsigned)faixa;
        float y = a.y + 30 + sorteio(&semente) % (unsigned)(a.height - 60);
        float raio = 4.0f + sorteio(&semente) % 9;
        DrawEllipse((int)x, (int)y, raio, raio * 0.6f, Fade(BLACK, 0.30f));
    }

    /* riscos verticais e poeira */
    int riscos = (sorteio(&semente) % 100 < 35) ? 1 + (int)(sorteio(&semente) % 2) : 0;
    for (int i = 0; i < riscos; i++) {
        int x = (int)(a.x + 8 + sorteio(&semente) % (unsigned)(a.width - 16));
        DrawRectangle(x, (int)a.y, 1, (int)a.height, Fade(COR_CREME, 0.22f));
    }
    int poeira = 2 + (int)(sorteio(&semente) % 4);
    for (int i = 0; i < poeira; i++) {
        float x = a.x + 6 + sorteio(&semente) % (unsigned)(a.width - 12);
        float y = a.y + 6 + sorteio(&semente) % (unsigned)(a.height - 12);
        DrawCircleV((Vector2){ x, y }, 1.0f + sorteio(&semente) % 2, Fade(BLACK, 0.45f));
    }

    /* bordas escurecidas: faixas finas, cada uma um pouco mais clara */
    int ax = (int)a.x, ay = (int)a.y, aw = (int)a.width, ah = (int)a.height;
    for (int i = 0; i < 20; i++) {
        float f = 1.0f - i / 20.0f;
        Color sombra = Fade(BLACK, 0.42f * f * f);
        DrawRectangle(ax + i * 4, ay, 4, ah, sombra);
        DrawRectangle(ax + aw - (i + 1) * 4, ay, 4, ah, sombra);
        DrawRectangle(ax, ay + i * 3, aw, 3, sombra);
        DrawRectangle(ax, ay + ah - (i + 1) * 3, aw, 3, sombra);
    }

    /* cantos arredondados da janela do projetor */
    float r = 18.0f;
    DrawRing((Vector2){ a.x + r, a.y + r }, r, r * 1.6f, 180, 270, 10, BLACK);
    DrawRing((Vector2){ a.x + a.width - r, a.y + r }, r, r * 1.6f, 270, 360, 10, BLACK);
    DrawRing((Vector2){ a.x + a.width - r, a.y + a.height - r }, r, r * 1.6f, 0, 90, 10, BLACK);
    DrawRing((Vector2){ a.x + r, a.y + a.height - r }, r, r * 1.6f, 90, 180, 10, BLACK);
}

/* O nitrato pegando fogo (derrota). */
static void desenhar_queima(const Jogo *j)
{
    static const Color CAMADAS[4] = {
        { 46, 22, 8, 255 }, { 214, 104, 28, 255 }, { 250, 208, 110, 255 }, { 255, 250, 236, 255 }
    };
    static const float PROPORCAO[4] = { 1.0f, 0.86f, 0.62f, 0.44f };
    Rectangle a = j->areaFilme;

    /* desenha camada por camada para as manchas se fundirem */
    for (int camada = 0; camada < 4; camada++) {
        unsigned semente = 4242u;
        for (int foco = 0; foco < 6; foco++) {
            float x = a.x + a.width * (0.15f + (sorteio(&semente) % 70) / 100.0f);
            float y = a.y + a.height * (0.15f + (sorteio(&semente) % 70) / 100.0f);
            float atraso = foco * 0.22f;
            float raio = (j->tempoTela - atraso) * 330.0f;
            if (raio > 0.0f) DrawCircleV((Vector2){ x, y }, raio * PROPORCAO[camada], CAMADAS[camada]);
        }
    }
}

/* Tapa tudo o que vazou para fora do vão do filme e desenha a borda. */
static void desenhar_mascara(void)
{
    int x = (int)VAO_FILME.x, y = (int)VAO_FILME.y;
    int w = (int)VAO_FILME.width, h = (int)VAO_FILME.height;
    DrawRectangle(0, 0, LARGURA_TELA, y, COR_FUNDO);
    DrawRectangle(0, y + h, LARGURA_TELA, ALTURA_TELA - y - h, COR_FUNDO);
    DrawRectangle(0, y, x, h, COR_FUNDO);
    DrawRectangle(x + w, y, LARGURA_TELA - x - w, h, COR_FUNDO);
    DrawRectangleLinesEx((Rectangle){ x - 3.0f, y - 3.0f, w + 6.0f, h + 6.0f }, 2.0f, COR_RELEVO);
}

/* ------------------------------------------------------------- controles */

static void desenhar_botao(const Jogo *j, int i)
{
    static const char *const ROTULOS[4] = { "RETROCEDER", "PAUSAR", "PLAY", "AVANÇAR" };

    Rectangle r = rect_botao(i);
    bool emJogo = (j->tela == TELA_JOGO);
    bool ativo = emJogo && ((i == BOTAO_PAUSAR && j->projetor == PROJETOR_PAUSA) ||
                            (i == BOTAO_PLAY && j->projetor == PROJETOR_PLAY));
    bool sobre = emJogo && CheckCollisionPointRec(j->mouse, r);

    /* fica vermelho o botão que, apertado agora, queimaria o filme */
    bool paraOFilme = (i == BOTAO_PAUSAR) ||
                      (PASSO_GASTA_PAUSA && (i == BOTAO_RETROCEDER || i == BOTAO_AVANCAR));
    bool perigo = paraOFilme && j->projetor == PROJETOR_PLAY && j->pausasUsadas >= MAX_PAUSAS;

    Color fundo = ativo ? COR_OURO : (sobre ? COR_RELEVO : COR_PAINEL);
    Color tinta = ativo ? COR_FUNDO : (perigo ? COR_SANGUE : COR_CREME);
    DrawRectangleRec(r, fundo);
    DrawRectangleLinesEx(r, 2.0f, perigo ? COR_SANGUE : COR_OURO);

    float cx = r.x + 28.0f, cy = r.y + r.height / 2.0f;
    switch (i) {
    case BOTAO_RETROCEDER:                      /* barra + seta: um quadro para trás */
        DrawRectangle((int)cx - 10, (int)cy - 8, 4, 16, tinta);
        seta_esquerda(cx - 4, cy, 13, 8, tinta);
        break;
    case BOTAO_PAUSAR:
        DrawRectangle((int)cx - 8, (int)cy - 8, 6, 16, tinta);
        DrawRectangle((int)cx + 2, (int)cy - 8, 6, 16, tinta);
        break;
    case BOTAO_PLAY:
        seta_direita(cx - 6, cy, 14, 9, tinta);
        break;
    case BOTAO_AVANCAR:                         /* seta + barra: um quadro para a frente */
        seta_direita(cx - 9, cy, 13, 8, tinta);
        DrawRectangle((int)cx + 6, (int)cy - 8, 4, 16, tinta);
        break;
    }

    float areaTexto = r.width - 52.0f;
    escrever_centro(j, FONTE_UI, ROTULOS[i], r.x + 48.0f + areaTexto / 2.0f, r.y + 11.0f, 19, tinta);
}

static void desenhar_linha_do_tempo(const Jogo *j)
{
    Rectangle l = LINHA_TEMPO;
    float porQuadro = l.width / j->filme->total;

    DrawRectangleRec(l, COR_RELEVO);

    /* quadros em que o jogador já registrou anomalias */
    for (const Anomalia *a = j->anomalias->inicio; a; a = a->prox) {
        if (!a->coletada) continue;
        const Quadro *onde = filme_buscar(j->filme, a->quadroColeta);
        if (!onde) continue;
        DrawRectangle((int)(l.x + onde->indice * porQuadro), (int)l.y,
                      (int)porQuadro + 1, (int)l.height, COR_OURO);
    }

    float x = l.x + (j->filme->atual->indice + 0.5f) * porQuadro;
    DrawRectangle((int)x - 2, (int)l.y - 4, 4, (int)l.height + 8, COR_CREME);
}

/* -------------------------------------------------------- painel: dossiê */

static void desenhar_dossie(const Jogo *j)
{
    float x = PAINEL.x + 20.0f, direita = PAINEL.x + PAINEL.width - 20.0f;
    float largura = PAINEL.width - 40.0f;
    char texto[160];

    DrawRectangleRec(PAINEL, COR_PAINEL);
    DrawRectangleLinesEx(PAINEL, 1.0f, COR_RELEVO);

    escrever(j, FONTE_UI, "DOSSIÊ", x, PAINEL.y + 16, 30, COR_CREME);
    escrever_direita(j, FONTE_MAQUINA, "CASO Nº " NUMERO_CASO, direita, PAINEL.y + 26, 16, COR_APAGADO);
    DrawRectangle((int)x, (int)PAINEL.y + 58, (int)largura, 1, COR_OURO);

    /* três indicadores: tempo, anomalias, pausas */
    float y = PAINEL.y + 76;
    float coluna = largura / 3.0f;
    escrever(j, FONTE_MAQUINA, "TEMPO", x, y, 14, COR_APAGADO);
    escrever(j, FONTE_MAQUINA, "ANOMALIAS", x + coluna, y, 14, COR_APAGADO);
    escrever(j, FONTE_MAQUINA, "PAUSAS", x + coluna * 2, y, 14, COR_APAGADO);

    escrever(j, FONTE_UI, relogio(jogo_tempo_final(j)), x, y + 20, 30, COR_CREME);
    escrever(j, FONTE_UI, TextFormat("%d/%d", j->coletadas, j->totalAtivas),
             x + coluna, y + 20, 30, COR_OURO);

    int usadas = j->pausasUsadas > MAX_PAUSAS ? MAX_PAUSAS : j->pausasUsadas;
    for (int i = 0; i < MAX_PAUSAS; i++) {
        /* as pausas gastas ficam apagadas e riscadas */
        bool gasta = (i >= MAX_PAUSAS - usadas);
        float px = x + coluna * 2 + i * 30.0f, py = y + 24;
        Color cor = gasta ? COR_RELEVO : COR_CREME;
        DrawRectangle((int)px, (int)py, 7, 22, cor);
        DrawRectangle((int)px + 11, (int)py, 7, 22, cor);
        if (gasta) DrawLineEx((Vector2){ px - 3, py + 22 }, (Vector2){ px + 21, py }, 2.0f, COR_SANGUE);
    }

    y += 66;
    if (j->pausasUsadas >= MAX_PAUSAS) {
        escrever(j, FONTE_MAQUINA, "Parar o filme de novo vai queimá-lo.", x, y, 16, COR_SANGUE);
    } else if (j->erros > 0) {
        snprintf(texto, sizeof texto, "Alarmes falsos: %d  (+%d s)", j->erros,
                 (int)(j->erros * PENALIDADE_ERRO));
        escrever(j, FONTE_MAQUINA, texto, x, y, 16, COR_APAGADO);
    } else {
        snprintf(texto, sizeof texto, "Volta nº %d do rolo", j->sessao);
        escrever(j, FONTE_MAQUINA, texto, x, y, 16, COR_APAGADO);
    }

    y += 34;
    DrawRectangle((int)x, (int)y, (int)largura, 1, COR_RELEVO);
    y += 14;
    escrever(j, FONTE_UI, "EVIDÊNCIAS", x, y, 20, COR_CREME);
    y += 36;

    /* uma linha por anomalia do caso, na ordem em que foram registradas */
    for (int linha = 1; linha <= j->totalAtivas && linha <= MAX_EVIDENCIAS; linha++) {
        const Anomalia *achada = NULL;
        for (const Anomalia *a = j->anomalias->inicio; a; a = a->prox)
            if (a->coletada && a->ordemColeta == linha) achada = a;

        escrever(j, FONTE_UI, TextFormat("%d", linha), x, y + 6, 22, achada ? COR_OURO : COR_RELEVO);
        if (achada) {
            snprintf(texto, sizeof texto, "QUADRO %03d  ·  achada em %s",
                     achada->quadroColeta, relogio(achada->tempoColeta));
            escrever(j, FONTE_MAQUINA, texto, x + 30, y, 14, COR_OURO);
            encaixar(j, FONTE_MAQUINA, achada->descricao, 18, largura - 30, texto, sizeof texto);
            escrever(j, FONTE_MAQUINA, texto, x + 30, y + 18, 18, COR_CREME);
        } else {
            escrever(j, FONTE_MAQUINA, "QUADRO ???", x + 30, y, 14, COR_RELEVO);
            escrever(j, FONTE_MAQUINA, "não localizada", x + 30, y + 18, 18, COR_APAGADO);
        }
        y += 50;
    }
}

/* ---------------------------------------------------- a entidade (final) */

static void desenhar_entidade(const Jogo *j)
{
    float p = j->tempoTela / DURACAO_FUGA;              /* 0 no início, 1 no fim */
    if (p > 1.0f) p = 1.0f;
    Vector2 meio = { LARGURA_TELA / 2.0f, ALTURA_TELA / 2.0f };
    Vector2 centro = { j->origemFuga.x + (meio.x - j->origemFuga.x) * p,
                       j->origemFuga.y + (meio.y - j->origemFuga.y) * p };

    /* mancha de escuridão que nasce onde a última anomalia foi achada e
     * transborda para fora da tela de cinema */
    float raio = 14.0f + p * p * 1000.0f;
    for (int i = 0; i < 10; i++) {
        float angulo = i * 36.0f * DEG2RAD + j->tempoTela * 0.8f;
        float pulso = 0.70f + 0.08f * sinf(j->tempoTela * 9.0f + i * 1.7f);
        DrawCircleV((Vector2){ centro.x + cosf(angulo) * raio * pulso,
                               centro.y + sinf(angulo) * raio * pulso }, raio * 0.45f, BLACK);
    }
    DrawCircleV(centro, raio, BLACK);

    if (j->temEntidade) {
        /* arte própria: assets/entidade.png cresce no meio da escuridão */
        float alturaTela = ALTURA_TELA * (0.15f + p * 0.85f);
        float escala = alturaTela / j->entidade.height;
        float w = j->entidade.width * escala, h = j->entidade.height * escala;
        float alfa = (p < 0.25f) ? p * 4.0f : 1.0f;
        if (p > 0.85f) alfa = (1.0f - p) / 0.15f;
        DrawTexturePro(j->entidade,
                       (Rectangle){ 0, 0, (float)j->entidade.width, (float)j->entidade.height },
                       (Rectangle){ centro.x - w / 2, centro.y - h / 2, w, h },
                       (Vector2){ 0, 0 }, 0.0f, Fade(WHITE, alfa));
    } else {
        /* placeholder: dois olhos que se abrem e, no fim, se fecham */
        float distancia = fminf(raio * 0.36f, 150.0f);
        float largura = fminf(raio * 0.22f, 92.0f);
        float abertura = fminf(p * 3.0f, 1.0f);
        if (p > 0.85f) abertura = (1.0f - p) / 0.15f;
        float altura = largura * 0.42f * abertura;
        if (altura > 0.5f) {
            DrawEllipse((int)(centro.x - distancia), (int)centro.y, largura, altura, COR_CREME);
            DrawEllipse((int)(centro.x + distancia), (int)centro.y, largura, altura, COR_CREME);
            DrawEllipse((int)(centro.x - distancia), (int)centro.y, largura * 0.16f, altura, BLACK);
            DrawEllipse((int)(centro.x + distancia), (int)centro.y, largura * 0.16f, altura, BLACK);
        }
    }

    if (j->tempoTela > 0.9f && p < 0.85f) {
        const char *frase = "ELA ESTÁ SAINDO DO FILME";
        float alfa = fminf((j->tempoTela - 0.9f) * 2.0f, 1.0f);
        float largura = medir(j, FONTE_UI, frase, 30) + 56.0f;
        DrawRectangleRec((Rectangle){ meio.x - largura / 2.0f, 566.0f, largura, 52.0f }, Fade(BLACK, alfa));
        escrever_centro(j, FONTE_UI, frase, meio.x, 578.0f, 30, Fade(COR_CREME, alfa));
    }
}

/* ---------------------------------------------------------- tela de jogo */

static void desenhar_partida(const Jogo *j)
{
    ClearBackground(COR_FUNDO);

    /* 1. o filme (e o que acontece dentro dele) */
    float dx = 0.0f, dy = 0.0f;
    if (j->tremor > 0.0f) dx = sinf(j->tremor * 80.0f) * 6.0f * (j->tremor / 0.3f);
    if (j->tela == TELA_FUGA) {
        float forca = 2.0f + j->tempoTela * 3.0f;
        dx = sinf(j->tempoTela * 71.0f) * forca;
        dy = cosf(j->tempoTela * 53.0f) * forca;
    }
    desenhar_quadro(j, dx, dy);
    if (j->tela == TELA_DERROTA) desenhar_queima(j);
    desenhar_mascara();

    /* 2. cabeçalho */
    escrever(j, FONTE_MAQUINA, "“" TITULO_FILME "” (" ANO_FILME ")", VAO_FILME.x, 14, 18, COR_APAGADO);
    escrever_direita(j, FONTE_MAQUINA,
                     TextFormat("QUADRO %03d / %03d", j->filme->atual->numero, j->filme->total),
                     VAO_FILME.x + VAO_FILME.width, 14, 18, COR_CREME);

    /* 3. controles e painel */
    desenhar_linha_do_tempo(j);
    for (int i = 0; i < 4; i++) desenhar_botao(j, i);
    desenhar_dossie(j);

    /* 4. retorno dos cliques */
    if (j->marcadorTempo > 0.0f) {
        float t = 1.0f - j->marcadorTempo / 0.6f;       /* 0 -> 1 */
        if (j->marcadorBom) {
            DrawRing(j->marcadorPos, 10 + t * 38, 14 + t * 38, 0, 360, 40, Fade(COR_OURO, 1.0f - t));
            DrawRing(j->marcadorPos, 4 + t * 20, 6 + t * 20, 0, 360, 40, Fade(COR_CREME, 1.0f - t));
        } else {
            float s = 12.0f;
            Vector2 p = j->marcadorPos;
            DrawLineEx((Vector2){ p.x - s, p.y - s }, (Vector2){ p.x + s, p.y + s }, 3, Fade(COR_SANGUE, 1.0f - t));
            DrawLineEx((Vector2){ p.x - s, p.y + s }, (Vector2){ p.x + s, p.y - s }, 3, Fade(COR_SANGUE, 1.0f - t));
        }
    }
    if (j->avisoTempo > 0.0f && j->tela == TELA_JOGO) {
        float alfa = fminf(j->avisoTempo * 3.0f, 1.0f);
        float largura = medir(j, FONTE_UI, j->aviso, 24) + 44.0f;
        float cx = VAO_FILME.x + VAO_FILME.width / 2.0f;
        Rectangle faixa = { cx - largura / 2.0f, VAO_FILME.y + 22.0f, largura, 40.0f };
        Color cor = j->avisoBom ? COR_OURO : COR_SANGUE;
        DrawRectangleRec(faixa, Fade(COR_FUNDO, 0.88f * alfa));
        DrawRectangleLinesEx(faixa, 1.0f, Fade(cor, alfa));
        escrever_centro(j, FONTE_UI, j->aviso, cx, faixa.y + 9.0f, 24, Fade(cor, alfa));
    }

    /* 5. lupa do investigador sobre a tela de cinema */
    if (j->tela == TELA_JOGO && CheckCollisionPointRec(j->mouse, j->areaFilme)) {
        DrawRing(j->mouse, 16, 18, 0, 360, 40, COR_OURO);
        DrawLineEx((Vector2){ j->mouse.x + 12, j->mouse.y + 12 },
                   (Vector2){ j->mouse.x + 24, j->mouse.y + 24 }, 3.0f, COR_OURO);
    }

    /* 6. a entidade sai do filme, por cima de tudo */
    if (j->tela == TELA_FUGA) desenhar_entidade(j);
}

/* --------------------------------------------------------- telas de texto */

static void desenhar_menu(const Jogo *j)
{
    static const char *const OPCOES[4] = {
        "INICIAR INVESTIGAÇÃO", "COMO JOGAR", "RANKING", "SAIR"
    };
    float cx = LARGURA_TELA / 2.0f;

    desenhar_moldura(j);
    escrever_centro(j, FONTE_MAQUINA, "ARQUIVO DE OCORRÊNCIAS PARANORMAIS  ·  CASO Nº " NUMERO_CASO,
                    cx, 96, 18, COR_APAGADO);
    escrever_centro(j, FONTE_TITULO, TITULO_JOGO, cx, 132, 112, COR_CREME);
    desenhar_ornamento(268);
    escrever_centro(j, FONTE_MAQUINA,
                    "Há algo escondido entre os quadros de “" TITULO_FILME "” (" ANO_FILME ").",
                    cx, 292, 20, COR_CREME);
    desenhar_opcoes(j, OPCOES, 4, MENU_Y);
    escrever_centro(j, FONTE_MAQUINA, "SETAS ou MOUSE para escolher  ·  ENTER para confirmar",
                    cx, 632, 16, COR_APAGADO);
}

static void desenhar_como_jogar(const Jogo *j)
{
    static const char *const VOLTAR[1] = { "VOLTAR" };
    const char *linhas[8];
    char ritmo[128], chance[128], pausas[128], multa[128];

    snprintf(ritmo, sizeof ritmo,
             "O filme roda a %d quadros por segundo e recomeça quando o rolo acaba.", FPS_FILME);
    snprintf(chance, sizeof chance,
             "A cada investigação, cada anomalia tem %d%% de chance de estar escondida no filme.",
             CHANCE_ANOMALIA);
    snprintf(pausas, sizeof pausas,
             "O filme só pode ser parado %d vezes. Na vez seguinte, ele queima.", MAX_PAUSAS);
    snprintf(multa, sizeof multa,
             "Cada clique errado soma %d segundos ao seu tempo final.", (int)PENALIDADE_ERRO);

    linhas[0] = ritmo;
    linhas[1] = chance;
    linhas[2] = "Cada anomalia altera um único quadro, sempre o mesmo, até ser registrada.";
    linhas[3] = "Clique sobre a anomalia quando ela estiver na tela para registrá-la.";
    linhas[4] = "RETROCEDER e AVANÇAR param o filme e andam um quadro por clique.";
    linhas[5] = pausas;
    linhas[6] = multa;
    linhas[7] = "Registre todas as anomalias para expulsar a entidade do filme.";

    desenhar_moldura(j);
    escrever_centro(j, FONTE_TITULO, "COMO INVESTIGAR", LARGURA_TELA / 2.0f, 70, 58, COR_CREME);
    desenhar_ornamento(150);

    for (int i = 0; i < 8; i++) {
        float y = 182.0f + i * 40.0f;
        losango(176, y + 10, 5, COR_OURO);
        escrever(j, FONTE_MAQUINA, linhas[i], 198, y, 20, COR_CREME);
    }
    escrever_centro(j, FONTE_MAQUINA,
                    "ESPAÇO para e solta o filme  ·  SETAS ESQUERDA e DIREITA andam um quadro  ·  ESC sai",
                    LARGURA_TELA / 2.0f, 528, 17, COR_APAGADO);
    desenhar_opcoes(j, VOLTAR, 1, 600.0f);
}

static void desenhar_ranking(const Jogo *j)
{
    static const char *const VOLTAR[1] = { "VOLTAR" };
    const float colPos = 270, colNome = 340, colTempo = 660, colPausas = 800, colErros = 1010;

    desenhar_moldura(j);
    escrever_centro(j, FONTE_TITULO, "CASOS ENCERRADOS", LARGURA_TELA / 2.0f, 70, 58, COR_CREME);
    desenhar_ornamento(150);

    if (j->ranking.total == 0) {
        escrever_centro(j, FONTE_MAQUINA, "Nenhum caso foi encerrado ainda.",
                        LARGURA_TELA / 2.0f, 330, 22, COR_APAGADO);
    } else {
        escrever(j, FONTE_MAQUINA, "INVESTIGADOR", colNome, 178, 15, COR_APAGADO);
        escrever_direita(j, FONTE_MAQUINA, "TEMPO", colTempo, 178, 15, COR_APAGADO);
        escrever_direita(j, FONTE_MAQUINA, "PAUSAS", colPausas, 178, 15, COR_APAGADO);
        escrever_direita(j, FONTE_MAQUINA, "ALARMES FALSOS", colErros, 178, 15, COR_APAGADO);
        DrawRectangle(260, 200, 760, 1, COR_RELEVO);
    }
    for (int i = 0; i < j->ranking.total; i++) {
        const Registro *r = &j->ranking.itens[i];
        float y = 212.0f + i * 36.0f;
        Color cor = (i == j->posicaoRanking) ? COR_OURO : COR_CREME;

        if (i == j->posicaoRanking) seta_direita(colPos - 28, y + 10, 12, 8, COR_OURO);
        escrever(j, FONTE_MAQUINA, TextFormat("%2dº", i + 1), colPos, y, 22, cor);
        escrever(j, FONTE_MAQUINA, r->nome, colNome, y, 22, cor);
        escrever_direita(j, FONTE_MAQUINA, relogio(r->tempo), colTempo, y, 22, cor);
        escrever_direita(j, FONTE_MAQUINA, TextFormat("%d", r->pausas), colPausas, y, 22, cor);
        escrever_direita(j, FONTE_MAQUINA, TextFormat("%d", r->erros), colErros, y, 22, cor);
    }
    desenhar_opcoes(j, VOLTAR, 1, 600.0f);
}

static void desenhar_vitoria(const Jogo *j)
{
    static const char *const ARQUIVAR[1] = { "ARQUIVAR O CASO" };
    float cx = LARGURA_TELA / 2.0f;
    char texto[160];

    desenhar_moldura(j);
    escrever_centro(j, FONTE_TITULO, "CASO ENCERRADO", cx, 70, 64, COR_CREME);
    desenhar_ornamento(156);
    escrever_centro(j, FONTE_MAQUINA, "A entidade abandonou o filme.", cx, 184, 22, COR_CREME);

    escrever_centro(j, FONTE_MAQUINA, "TEMPO FINAL", cx, 244, 15, COR_APAGADO);
    escrever_centro(j, FONTE_TITULO, relogio(jogo_tempo_final(j)), cx, 264, 54, COR_OURO);

    if (j->erros > 0)
        snprintf(texto, sizeof texto, "%s de investigação  +  %d s por %d %s",
                 relogio(j->tempo), (int)(j->erros * PENALIDADE_ERRO), j->erros,
                 j->erros == 1 ? "alarme falso" : "alarmes falsos");
    else
        snprintf(texto, sizeof texto, "Nenhum alarme falso.");
    escrever_centro(j, FONTE_MAQUINA, texto, cx, 332, 17, COR_APAGADO);
    snprintf(texto, sizeof texto, "Anomalias registradas: %d   ·   Pausas usadas: %d de %d",
             j->coletadas, j->pausasUsadas, MAX_PAUSAS);
    escrever_centro(j, FONTE_MAQUINA, texto, cx, 358, 17, COR_APAGADO);

    escrever_centro(j, FONTE_MAQUINA, "Assine o relatório:", cx, 420, 20, COR_CREME);
    Rectangle campo = { cx - 190, 452, 380, 54 };
    DrawRectangleRec(campo, COR_PAINEL);
    DrawRectangleLinesEx(campo, 2.0f, COR_OURO);
    bool cursor = fmodf(j->tempoTela, 1.0f) < 0.5f;
    snprintf(texto, sizeof texto, "%s%s", j->nome, cursor ? "_" : " ");
    escrever_centro(j, FONTE_UI, texto, cx, campo.y + 12, 32, COR_CREME);

    desenhar_opcoes(j, ARQUIVAR, 1, 560.0f);
}

static void desenhar_derrota(const Jogo *j)
{
    static const char *const OPCOES[2] = { "TENTAR DE NOVO", "VOLTAR AO MENU" };
    float cx = LARGURA_TELA / 2.0f;
    char texto[160];

    if (j->tempoTela < DURACAO_QUEIMA) {        /* primeiro, o filme pega fogo */
        desenhar_partida(j);
        return;
    }

    desenhar_moldura(j);
    escrever_centro(j, FONTE_TITULO, "O FILME QUEIMOU", cx, 96, 64, COR_SANGUE);
    desenhar_ornamento(184);
    snprintf(texto, sizeof texto, "Você parou o projetor mais de %d vezes.", MAX_PAUSAS);
    escrever_centro(j, FONTE_MAQUINA, texto, cx, 226, 22, COR_CREME);
    escrever_centro(j, FONTE_MAQUINA,
                    "O nitrato não perdoa: a película ardeu e a entidade continua lá dentro.",
                    cx, 262, 22, COR_CREME);
    snprintf(texto, sizeof texto, "Anomalias registradas: %d de %d", j->coletadas, j->totalAtivas);
    escrever_centro(j, FONTE_MAQUINA, texto, cx, 340, 18, COR_APAGADO);

    desenhar_opcoes(j, OPCOES, 2, 480.0f);
}

static void desenhar_erro(const Jogo *j)
{
    static const char *const SAIR[1] = { "SAIR" };
    float cx = LARGURA_TELA / 2.0f;

    desenhar_moldura(j);
    escrever_centro(j, FONTE_TITULO, "ROLO NÃO ENCONTRADO", cx, 110, 56, COR_SANGUE);
    desenhar_ornamento(196);
    escrever_centro(j, FONTE_MAQUINA, j->erro, cx, 260, 20, COR_CREME);
    escrever_centro(j, FONTE_MAQUINA,
                    "Veja no LEIAME.md como organizar a pasta assets/ e rode o jogo a partir da pasta dele.",
                    cx, 310, 18, COR_APAGADO);
    desenhar_opcoes(j, SAIR, 1, 520.0f);
}

void jogo_desenhar(const Jogo *j)
{
    switch (j->tela) {
    case TELA_MENU:       desenhar_menu(j); break;
    case TELA_COMO_JOGAR: desenhar_como_jogar(j); break;
    case TELA_RANKING:    desenhar_ranking(j); break;
    case TELA_JOGO:
    case TELA_FUGA:       desenhar_partida(j); break;
    case TELA_VITORIA:    desenhar_vitoria(j); break;
    case TELA_DERROTA:    desenhar_derrota(j); break;
    case TELA_ERRO:       desenhar_erro(j); break;
    }
}
