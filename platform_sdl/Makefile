CXX ?= clang++
CXXFLAGS ?= -std=c++17 -I..

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)

ifeq ($(strip $(SDL_CFLAGS)),)
  SDL_CFLAGS := -I/opt/homebrew/include/SDL2 -D_THREAD_SAFE
  SDL_LIBS := -L/opt/homebrew/lib -lSDL2
endif

SDL_GFX_CFLAGS := $(shell pkg-config --cflags SDL2_gfx 2>/dev/null)
SDL_GFX_LIBS := $(shell pkg-config --libs SDL2_gfx 2>/dev/null)

ifeq ($(strip $(SDL_GFX_CFLAGS)),)
  SDL_GFX_CFLAGS := -I/opt/homebrew/include/SDL2
  SDL_GFX_LIBS := -L/opt/homebrew/lib -lSDL2_gfx
endif

TARGET := miniacid
SOURCES := ../dsp_engine.cpp ../miniacid_display.cpp ../cardputer_display.cpp ../scenes.cpp ../json_evented.cpp sdl_main.cpp sdl_display.cpp scene_storage_sdl.cpp 

ROOT := $(abspath ..)
DOCKER ?= docker
EMCC_IMAGE ?= emscripten/emsdk
WASM_FLAGS := -I.. -std=c++17 -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=262144 -sASSERTIONS=1 -sUSE_SDL_GFX=2 -sEXPORTED_RUNTIME_METHODS='[UTF8ToString,stringToUTF8,lengthBytesUTF8]'

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(SDL_GFX_CFLAGS) $^ $(SDL_LIBS) $(SDL_GFX_LIBS) -o $@

wasm: $(SOURCES)
	mkdir -p $(ROOT)/web
	$(DOCKER) run --rm -v $(ROOT):/src -w /src/platform_sdl $(EMCC_IMAGE) emcc $(SOURCES) $(WASM_FLAGS) -o /src/web/miniacid.html

clean:
	rm -f $(TARGET)

.PHONY: all clean wasm
