COMP=clang
OPTIONS=-Wall -Wextra -Werror -Wno-unused-parameter
OPTIONS_WEB=-I$(RAYLIB_SRC) -L$(RAYLIB_SRC) -sUSE_GLFW=3 -sGL_ENABLE_GET_PROC_ADDRESS -DPLATFORM_WEB --shell-file shell.html
DEBUG=-fsanitize=address,undefined -g3
LIBS=-lraylib

SOURCES=main.c object.c
OUTPUT=asteroids
OUTPUT_WEB=index.html

final:
	emcc $(OPTIONS) $(SOURCES) $(RAYLIB_SRC)/libraylib.a $(OPTIONS_WEB) -o $(OUTPUT_WEB)

debug:
	emcc $(OPTIONS) $(DEBUG) $(SOURCES) $(RAYLIB_SRC)/libraylib.a $(OPTIONS_WEB) -o $(OUTPUT_WEB)

final-desktop:
	$(COMP) $(OPTIONS) $(LIBS) $(SOURCES) -o $(OUTPUT)

debug-desktop:
	$(COMP) $(OPTIONS) $(LIBS) $(DEBUG) $(SOURCES) -o $(OUTPUT)
