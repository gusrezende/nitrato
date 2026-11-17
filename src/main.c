/*
 * NITRATO — jogo de investigação paranormal em um filme mudo de 1927.
 *
 * main.c cuida só da janela: abre, lê teclado e mouse, e repassa tudo para
 * o jogo (jogo.c). O jogo é desenhado em uma tela lógica de 1280x720, que
 * depois é esticada para o tamanho real da janela — assim ela pode ser
 * redimensionada ou colocada em tela cheia (F11) sem quebrar o layout.
 */
#include "raylib.h"
#include "jogo.h"

#include <math.h>

/* Monta a struct Entrada com o que o jogador fez neste instante. */
static Entrada ler_entrada(float escala, float margemX, float margemY)
{
    Entrada e = { 0 };
    Vector2 mouse = GetMousePosition();

    e.dt = GetFrameTime();
    e.mouse.x = (mouse.x - margemX) / escala;   /* janela -> tela lógica */
    e.mouse.y = (mouse.y - margemY) / escala;
    e.clique = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    e.cima = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    e.baixo = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    e.esquerda = IsKeyPressed(KEY_LEFT);
    e.direita = IsKeyPressed(KEY_RIGHT);
    e.confirmar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    e.espaco = IsKeyPressed(KEY_SPACE);
    e.voltar = IsKeyPressed(KEY_ESCAPE);
    e.apagar = IsKeyPressed(KEY_BACKSPACE);
    e.caractere = GetCharPressed();
    return e;
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(LARGURA_TELA, ALTURA_TELA, TITULO_JOGO);
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL);                       /* ESC volta ao menu em vez de fechar */
    SetTargetFPS(60);
    ChangeDirectory(GetApplicationDirectory()); /* acha assets/ ao lado do executável */

    BeginDrawing();
    ClearBackground(BLACK);
    DrawText("Carregando o rolo de filme...", 40, ALTURA_TELA - 60, 20, RAYWHITE);
    EndDrawing();

    RenderTexture2D tela = LoadRenderTexture(LARGURA_TELA, ALTURA_TELA);
    SetTextureFilter(tela.texture, TEXTURE_FILTER_BILINEAR);

    Jogo jogo;
    jogo_iniciar(&jogo, "assets");

    while (!WindowShouldClose() && !jogo.sair) {
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        /* maior escala em que a tela lógica cabe inteira na janela */
        float escala = fminf((float)GetScreenWidth() / LARGURA_TELA,
                             (float)GetScreenHeight() / ALTURA_TELA);
        float margemX = (GetScreenWidth() - LARGURA_TELA * escala) / 2.0f;
        float margemY = (GetScreenHeight() - ALTURA_TELA * escala) / 2.0f;

        Entrada entrada = ler_entrada(escala, margemX, margemY);
        jogo_atualizar(&jogo, &entrada);

        BeginTextureMode(tela);
        jogo_desenhar(&jogo);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        /* altura negativa: texturas de renderização ficam de cabeça para baixo */
        DrawTexturePro(tela.texture,
                       (Rectangle){ 0, 0, LARGURA_TELA, -ALTURA_TELA },
                       (Rectangle){ margemX, margemY, LARGURA_TELA * escala, ALTURA_TELA * escala },
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    jogo_encerrar(&jogo);
    UnloadRenderTexture(tela);
    CloseWindow();
    return 0;
}
