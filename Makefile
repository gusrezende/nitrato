# NITRATO — Makefile
#
#   make              compila o jogo
#   make run          compila e executa
#   make teste        testa as listas e as ordenações no terminal
#   make clean        apaga os executáveis
#
# Requer a raylib (https://www.raylib.com). Se ela não estiver nas pastas
# padrão do compilador, informe onde estão raylib.h e libraylib.a:
#
#   make RAYLIB_DIR=C:/raylib/raylib/src

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2
FONTES  = src/main.c src/jogo.c src/filme.c src/anomalia.c src/ranking.c
LOGICA  = src/filme.c src/anomalia.c src/ranking.c

ifdef RAYLIB_DIR
    CFLAGS  += -I$(RAYLIB_DIR)
    LDFLAGS += -L$(RAYLIB_DIR)
endif

ifeq ($(OS),Windows_NT)
    EXE  = .exe
    LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
else ifeq ($(shell uname -s),Darwin)
    EXE  =
    LIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else
    EXE  =
    LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

all: nitrato$(EXE)

nitrato$(EXE): $(FONTES) src/*.h
	$(CC) $(CFLAGS) $(FONTES) -o $@ $(LDFLAGS) $(LIBS)

run: nitrato$(EXE)
	./nitrato$(EXE)

teste: testes/teste_estruturas.c $(LOGICA) src/*.h
	$(CC) $(CFLAGS) testes/teste_estruturas.c $(LOGICA) -o teste_estruturas$(EXE)
	./teste_estruturas$(EXE)

clean:
	rm -f nitrato$(EXE) teste_estruturas$(EXE)

.PHONY: all run teste clean
