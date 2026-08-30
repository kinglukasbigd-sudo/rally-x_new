# New Rally-X -- clean-room reconstruction (Phase 1)
#
# Build:  make          -> build/newrallyx
# Run:    make run
# Debug:  make BUILD=debug

CXX      ?= g++
BUILD    ?= release

SDL_PREFIX ?= third_party/sdl2
ifneq ($(wildcard $(SDL_PREFIX)/include/SDL2/SDL.h),)
  # Both paths: <SDL.h> resolves in SDL2/, and Debian's SDL_config.h shim
  # reaches back out for <SDL2/_real_SDL_config.h>.
  SDL_CFLAGS := -I$(SDL_PREFIX)/include/SDL2 -I$(SDL_PREFIX)/include
  SDL_LIBS   := -L$(SDL_PREFIX)/lib -lSDL2
else
  SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
  SDL_LIBS   := $(shell sdl2-config --libs   2>/dev/null || echo -lSDL2)
endif

WARN      := -Wall -Wextra -Wpedantic -Wshadow -Wno-unused-parameter
ifeq ($(BUILD),debug)
  OPT := -O0 -g -fsanitize=address,undefined
  LDF := -fsanitize=address,undefined
else
  OPT := -O2
  LDF :=
endif

CXXFLAGS := -std=c++17 $(WARN) $(OPT) -Isrc $(SDL_CFLAGS)
LDFLAGS  := $(LDF)
LDLIBS   := $(SDL_LIBS) -lm

SRCDIR   := src
OBJDIR   := build/obj/$(BUILD)
BIN      := build/newrallyx

SOURCES  := $(shell find $(SRCDIR) -name '*.cpp')
OBJECTS  := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))
DEPS     := $(OBJECTS:.o=.d)

.PHONY: all run clean levels test

all: $(BIN)

$(BIN): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)
	@echo "built $@"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

run: $(BIN)
	./$(BIN)

levels:
	python3 tools/genlevel.py levels

TEST_BIN     := build/rallyx_tests
TEST_SOURCES := $(wildcard tests/*.cpp)
GAME_OBJECTS := $(filter-out $(OBJDIR)/main.o,$(OBJECTS))
TEST_OBJECTS := $(patsubst tests/%.cpp,build/obj/tests/%.o,$(TEST_SOURCES))

build/obj/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Itests -MMD -MP -c $< -o $@

$(TEST_BIN): $(TEST_OBJECTS) $(GAME_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

-include $(TEST_OBJECTS:.o=.d)

clean:
	rm -rf build
