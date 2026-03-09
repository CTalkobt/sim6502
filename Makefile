CC       = gcc
CXX      = g++
CFLAGS   = -Wall -Wextra -O2 -I src -I src/core -I src/core/opcodes
CXXFLAGS = -Wall -Wextra -O2 -I src -I src/core -I src/core/opcodes

# --- Core Engine (Static Library) ---
CORE_SRCS = \
	src/core/cpu_engine.c \
	src/core/sim_api.c \
	src/core/interrupts.c \
	src/core/assembler.c \
	src/core/disassembler.c \
	src/core/condition.c \
	src/core/vic2.c \
	src/core/patterns.c

OPCODE_SRCS = \
	src/core/opcodes/6502.c \
	src/core/opcodes/6502_undoc.c \
	src/core/opcodes/65c02.c \
	src/core/opcodes/65ce02.c \
	src/core/opcodes/45gs02.c

CORE_OBJS = $(CORE_SRCS:.c=.o) $(OPCODE_SRCS:.c=.o)
LIB_TARGET = libsim6502.a

all: sim6502

$(LIB_TARGET): $(CORE_OBJS)
	ar rcs $@ $^

# --- CLI Frontend ---
CLI_SRCS = src/cli/main.c src/cli/commands.c
CLI_OBJS = $(CLI_SRCS:.c=.o)
TARGET   = sim6502

$(TARGET): $(CLI_OBJS) $(LIB_TARGET)
	$(CC) $(CFLAGS) -o $@ $^

# --- GUI Frontend ---
IMGUI_DIR  = src/gui/imgui
IMGUI_BACK = $(IMGUI_DIR)/backends

SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL2_LIBS   := $(shell pkg-config --libs   sdl2 2>/dev/null)
GL_LIBS      = -lGL

IMGUI_SRCS = \
	$(IMGUI_DIR)/imgui.cpp \
	$(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_BACK)/imgui_impl_sdl2.cpp \
	$(IMGUI_BACK)/imgui_impl_opengl3.cpp
IMGUI_OBJS = $(IMGUI_SRCS:.cpp=.o)

GUI_CXXFLAGS = $(CXXFLAGS) $(SDL2_CFLAGS) -I $(IMGUI_DIR) -I $(IMGUI_BACK)
GUI_TARGET   = sim6502-gui

src/gui/main.o: src/gui/main.cpp $(IMGUI_DIR)/imgui.h src/sim_api.h src/gui/imgui_filedlg.h
	$(CXX) $(GUI_CXXFLAGS) -c -o $@ $<

.PHONY: gui
gui: $(IMGUI_DIR)/imgui.h $(GUI_TARGET)

$(GUI_TARGET): src/gui/main.o $(LIB_TARGET) $(IMGUI_OBJS)
	$(CXX) -o $@ $^ $(SDL2_LIBS) $(GL_LIBS)

# --- Generic Rules ---
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(IMGUI_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	$(CXX) -O2 -I $(IMGUI_DIR) -I $(IMGUI_BACK) -c -o $@ $<

$(IMGUI_BACK)/%.o: $(IMGUI_BACK)/%.cpp
	$(CXX) -O2 $(SDL2_CFLAGS) -I $(IMGUI_DIR) -I $(IMGUI_BACK) -c -o $@ $<

# Auto-fetch ImGui (docking branch) if not present
$(IMGUI_DIR)/imgui.h:
	@echo "--- Dear ImGui not found; fetching docking branch ---"
	git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git $(IMGUI_DIR)

# --- Housekeeping ---
clean:
	rm -f $(CORE_OBJS) $(CLI_OBJS) $(TARGET) src/gui/main.o $(IMGUI_OBJS) $(GUI_TARGET) $(LIB_TARGET)

test: $(TARGET)
	./tools/run_tests.py

.PHONY: all clean gui test
