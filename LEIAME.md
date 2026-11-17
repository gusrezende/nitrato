# NITRATO

Jogo de investigação paranormal em C (raylib). Você analisa um filme animado
de 1927 que roda em laço. Alguns quadros estão alterados: são as anomalias.
Clique em todas para expulsar a entidade do filme — mas o projetor só pode
ser parado 3 vezes.

## Compilar e rodar

É preciso ter a [raylib](https://www.raylib.com) instalada (compila com as
versões 5.5 e 6.2).

```
make run
```

Se o compilador não encontrar a raylib, diga onde ela está:

```
make run RAYLIB_DIR=C:/raylib/raylib/src
```

Sem `make`, no Windows (MinGW / w64devkit):

```
gcc src/main.c src/jogo.c src/filme.c src/anomalia.c src/ranking.c -o nitrato.exe -std=c99 -lraylib -lopengl32 -lgdi32 -lwinmm
```

No Linux, troque o final por `-lraylib -lGL -lm -lpthread -ldl -lrt -lX11`.

O executável precisa ficar ao lado da pasta `assets/`.

## Como jogar

| Ação | Mouse | Teclado |
|---|---|---|
| Registrar anomalia | clicar sobre ela no filme | — |
| Parar / soltar o filme | botões PAUSAR e PLAY | ESPAÇO |
| Voltar um quadro | botão RETROCEDER | SETA ESQUERDA |
| Avançar um quadro | botão AVANÇAR | SETA DIREITA |
| Voltar ao menu | — | ESC |
| Tela cheia | — | F11 |

- O filme tem 11 quadros e roda a 11 quadros por segundo: uma volta por
  segundo.
- **No começo de cada partida, cada anomalia tem 60% de chance de estar
  escondida no filme** (pelo menos uma sempre está). O dossiê mostra quantas
  foram sorteadas.
- Cada anomalia sorteada altera **um único quadro**, e fica nesse mesmo
  quadro a partida inteira, até ser registrada. Duas anomalias nunca ocupam o
  mesmo quadro.
- Em outra partida, o sorteio é refeito: outras anomalias, outros quadros.
- **RETROCEDER e AVANÇAR andam exatamente um quadro por clique.** Se o filme
  estava rodando, ele para antes de dar o passo.
- O filme só pode ser parado 3 vezes por partida, seja com PAUSAR, seja com
  RETROCEDER/AVANÇAR. Com o filme já parado, os passos são livres. Parar pela
  4ª vez queima o filme: derrota.
- Com o filme rodando, o clique vale no quadro da anomalia e por mais 0,2
  segundo depois.
- Cada clique errado soma 5 segundos ao tempo final (vale para o ranking).
- Registrando todas as anomalias da partida, a entidade sai do filme.

## Regras e onde mudar

Tudo fica no começo de `src/jogo.h`:

| Constante | Valor | Significado |
|---|---|---|
| `FPS_FILME` | 11 | quadros por segundo da animação |
| `MAX_PAUSAS` | 3 | vezes que o filme pode ser parado |
| `PASSO_GASTA_PAUSA` | 1 | 1: RETROCEDER/AVANÇAR com o filme rodando gastam uma pausa; 0: param o filme de graça |
| `CHANCE_ANOMALIA` | 60 | chance (%) de cada anomalia estar no filme em uma partida |
| `TOLERANCIA_REACAO` | 0.20 | segundos em que o clique ainda vale depois que a anomalia sai da tela (0 = só no próprio quadro) |
| `PENALIDADE_ERRO` | 5.0 | segundos por clique errado |
| `FOLGA_CLIQUE` | 8.0 | tolerância do clique, em pixels do frame |
| `TITULO_JOGO`, `TITULO_FILME`, `ANO_FILME` | — | textos |

## A animação e as anomalias

```
assets/
  frames/                 frame_0001.png ... frame_0011.png   o filme
  anomalias/
    1_sem_cauda/          frame_0001.png ...   quadros alterados da anomalia
    2_rostos/             frame_0011.png
    ...
  anomalias.txt           descrição e áreas clicáveis de cada anomalia
  entidade.png            (opcional) a entidade que sai do filme no final
  fontes/                 fontes da interface
```

Os quadros vieram dos vídeos enviados, reduzidos para 800x450 (o tamanho em
que o jogo exibe o filme) e gravados em tons de cinza.

- **Filme**: os vídeos das anomalias trazem os quadros 8, 10 e 11 com o
  desenho um pouco diferente do primeiro vídeo. O filme usa essa versão mais
  nova, para que a única diferença de um quadro alterado seja a anomalia.
- **Anomalias**: cada quadro alterado é o quadro normal com apenas a região
  da anomalia trocada. As áreas clicáveis foram calculadas pela diferença
  entre o quadro alterado e o normal.

| Pasta | Anomalia | Quadros |
|---|---|---|
| `1_sem_cauda` | o bicho da direita perde a cauda | 1 a 11 |
| `2_rostos` | os rostos dos dois se deformam | só o 11 |
| `3_mao` | a mão do personagem da esquerda muda | 1 a 11 |
| `4_sem_luvas` | o bicho da direita perde as luvas | 1 a 11 |
| `5_caracol` | um caracol no chão (e respingos nos quadros 1 a 4) | 1 a 11 |
| `6_olhos` | o personagem da esquerda ganha olhos de gente | 1 a 11 |

### Formato de `assets/anomalias.txt`

```
anomalia 6_olhos Olhos de gente no personagem
quadro 1   277  245   78   71
quadro 2   275  240   77   71
area       100  300   60   60
```

- `anomalia <pasta> <descrição>` começa uma anomalia. A pasta fica em
  `assets/anomalias/`; a descrição aparece no dossiê.
- `quadro <número> <x> <y> <largura> <altura>` cadastra um quadro em que ela
  pode aparecer e a área clicável, em pixels da imagem. A imagem é o arquivo
  de mesmo número dentro da pasta.
- `area <x> <y> <largura> <altura>` acrescenta outra área clicável ao quadro
  da linha anterior (até 4 por quadro).

Para criar uma anomalia nova: faça a pasta, coloque nela os quadros
alterados com o mesmo nome dos originais e cadastre as linhas no `.txt`.
Quadros cadastrados sem imagem são ignorados, com aviso no terminal.

### Filtro de filme antigo

Os arquivos em `assets/` não têm filtro; quem envelhece a imagem é o jogo,
então o mesmo tratamento vale para o filme e para as anomalias.

| Etapa | Onde | O que faz |
|---|---|---|
| Revelação | `carregar_frame_antigo()` | ao carregar: preto e branco, contraste mais duro e viragem em sépia |
| Projeção | `desenhar_quadro()` | a cada quadro: granulado, riscos, poeira, manchas, cintilação, balanço da película, bordas escuras e cantos arredondados |

Os defeitos de projeção são sorteados a partir do contador de quadros, então
ficam parados quando o filme está pausado.

## Exigências do trabalho e onde cada uma está

| Exigência | Onde |
|---|---|
| Programado em C | todo o projeto (C99) |
| Limite de 3 pausas | `acionar_projetor()` em `jogo.c` |
| 11 quadros por segundo | `rodar_filme()` em `jogo.c` |
| 60% de chance de anomalia | `anomalias_sortear()` em `anomalia.c`, chamada por `jogo_nova_partida()` |
| Menu interativo | `TELA_MENU`: iniciar, como jogar, ranking, sair (mouse ou teclado) |
| Estrutura de dados na lógica central | o filme é uma lista duplamente encadeada circular; as anomalias, uma lista encadeada em que cada nó tem a sua lista de aparições |
| Ordenação além do ranking | merge sort monta o filme; insertion sort define a ordem em que as anomalias escolhem seus quadros |
| 6 ou mais funções da estrutura | 12 em `filme.h`, 15 em `anomalia.h` |
| Interface gráfica (bônus) | raylib |

### Estruturas de dados

**Rolo de filme — lista duplamente encadeada circular** (`filme.h`).
Cada nó (`Quadro`) é um frame. O projetor é o ponteiro `atual`: PLAY e AVANÇAR
fazem `atual = atual->prox`, RETROCEDER faz `atual = atual->ant`. É circular
porque o rolo recomeça ao chegar no fim, e duplamente encadeada porque o
filme roda nos dois sentidos. Cada nó aponta para a anomalia plantada nele
(ou `NULL`); é esse ponteiro que decide qual imagem aparece e se um clique
acerta.

**Dossiê — lista simplesmente encadeada de listas** (`anomalia.h`).
Cada nó (`Anomalia`) é uma anomalia do caso e guarda a sua própria lista
encadeada de `Aparicao`: os quadros alterados em que ela pode ser vista,
mantidos em ordem de quadro por inserção ordenada.

O que acontece no começo de cada partida (`jogo_nova_partida()`):

1. sorteio: cada anomalia entra na partida com 60% de chance;
2. a lista é ordenada: sorteadas na frente, começando pelas que têm menos
   quadros possíveis;
3. nessa ordem, cada anomalia escolhe ao acaso um quadro ainda livre do rolo
   e é plantada nele.

A ordem do passo 2 importa. A anomalia dos rostos só existe no quadro 11; se
outra anomalia escolhesse o 11 antes, ela ficaria sem lugar. Deixando as mais
limitadas escolherem primeiro, isso não acontece.

### Ordenação

| Algoritmo | Onde | Para quê |
|---|---|---|
| Merge sort (lista encadeada) | `filme_ordenar()` | o sistema entrega os arquivos em ordem qualquer; é a ordenação que monta o filme na sequência certa |
| Insertion sort (lista encadeada) | `anomalias_ordenar()` | põe as anomalias sorteadas na frente, da que tem menos quadros possíveis para a que tem mais (empate decidido por um número sorteado); é nessa ordem que elas ocupam os quadros do rolo |
| Insertion sort (vetor) | `ranking_ordenar()` | ranking por menor tempo |

### Principais funções

**Rolo de filme (`filme.c`)**

| Função | O que faz |
|---|---|
| `filme_criar()` | cria o rolo vazio |
| `filme_inserir(filme, numero, imagem)` | insere um quadro no fim, mantendo o círculo |
| `filme_ordenar(filme)` | merge sort por número do quadro; refaz `ant`, índices e o círculo |
| `filme_buscar(filme, numero)` | procura um quadro pelo número |
| `filme_avancar(filme)` | move o projetor para o próximo quadro |
| `filme_retroceder(filme)` | move o projetor para o quadro anterior |
| `filme_rebobinar(filme)` | volta o projetor ao primeiro quadro |
| `filme_plantar_anomalia(filme, anomalia, aparicao)` | faz um quadro do rolo exibir a versão alterada |
| `filme_plantar_em_quadro_livre(filme, anomalia)` | sorteia, entre os quadros da anomalia, um que esteja livre, e planta |
| `filme_remover_anomalia(filme, anomalia)` | tira a anomalia do rolo (coletada ou nova partida) |
| `filme_percorrer(filme, visita, dados)` | chama uma função para cada quadro |
| `filme_liberar(filme)` | libera todos os nós |

**Dossiê (`anomalia.c`)**

| Função | O que faz |
|---|---|
| `anomalias_criar()` | cria a lista vazia |
| `anomalias_inserir(lista, pasta, descricao)` | insere uma anomalia no fim |
| `anomalias_remover(lista, alvo)` | remove um nó (e as aparições dele) e religa a lista |
| `anomalias_carregar(lista, caminho)` | lê `anomalias.txt` |
| `anomalias_reiniciar(lista)` | zera a lista para uma nova partida |
| `anomalias_sortear(lista, chance)` | 60%: decide quais anomalias entram na partida |
| `anomalias_ordenar(lista)` | insertion sort: sorteadas primeiro, da mais limitada para a mais livre |
| `anomalias_contar_ativas(lista)` | quantas foram sorteadas |
| `anomalias_contar_coletadas(lista)` | quantas já foram achadas |
| `anomalias_liberar(lista)` | libera a lista e as sub-listas |
| `anomalia_adicionar_aparicao(anomalia, quadro)` | inserção ordenada na sub-lista |
| `anomalia_buscar_aparicao(anomalia, quadro)` | procura a aparição de um quadro |
| `anomalia_remover_aparicao(anomalia, alvo)` | remove uma aparição |
| `aparicao_adicionar_area(aparicao, ...)` | acrescenta uma área clicável |
| `aparicao_contem_ponto(aparicao, x, y, folga)` | o clique caiu em alguma área? |

**Jogo (`jogo.c`)**

| Função | O que faz |
|---|---|
| `jogo_iniciar(jogo, pasta)` | carrega fontes, frames, anomalias e ranking |
| `carregar_frame_antigo(caminho, ...)` | carrega um frame já com o filtro de filme antigo |
| `jogo_nova_partida(jogo)` | sorteia as anomalias da partida, ordena e planta cada uma em um quadro |
| `jogo_atualizar(jogo, entrada)` | aplica as regras da tela atual |
| `jogo_desenhar(jogo)` | desenha a tela atual |
| `acionar_projetor(jogo, estado, gastaPausa)` | para ou solta o filme e cobra o limite de pausas |
| `passo_do_projetor(jogo, direcao)` | RETROCEDER / AVANÇAR: anda um quadro na lista por clique |
| `rodar_filme(jogo, dt)` | com o filme solto, anda pela lista a 11 quadros por segundo |
| `investigar(jogo, mouse)` | confere se o clique acertou a anomalia |
| `jogo_encerrar(jogo)` | libera tudo |

**Ranking (`ranking.c`)**: `ranking_carregar`, `ranking_salvar`,
`ranking_ordenar`, `ranking_inserir`.

## Arquivos

```
src/main.c        janela, leitura de teclado e mouse, laço principal
src/jogo.c/.h     telas, regras e desenho
src/filme.c/.h    lista duplamente encadeada circular + merge sort
src/anomalia.c/.h lista de anomalias com sub-listas de aparições + insertion sort + sorteio
src/ranking.c/.h  ranking em arquivo + insertion sort em vetor
testes/           teste das listas e ordenações no terminal (make teste)
```

O ranking é gravado em `ranking.txt`, ao lado do executável.

## Créditos

Fontes: Limelight (SIL Open Font License) e Special Elite (Apache License 2.0),
do Google Fonts. As licenças estão em `assets/fontes/`.
