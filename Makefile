CC       = gcc
CXX      = g++
# Disable warnings for strncpy/strncat output truncation (e.g. copying x bytes from a string 
# of length y where x < y). This occurs when the destination buffer may not be null-terminated.
CFLAGS   = -Wall -Wextra -O2 -Wno-stringop-truncation
CXXFLAGS = -pthread -Wall -Wextra -O2 -Wno-stringop-truncation

# Include paths for library code (no src/ prefix — each lib is its own root)
LIB_IFLAGS = \
	-I src/lib6502-core \
	-I src/lib6502-core/opcodes \
	-I src/lib6502-mem \
	-I src/lib6502-devices \
	-I src/lib6502-devices/device \
	-I src/lib6502-debug \
	-I src/lib6502-toolchain \
	-I src/cli/commands

# Frontends also see src/ (for sim_api.h)
FRONT_IFLAGS = $(LIB_IFLAGS) -I src

# --- lib6502-core ---
CORE_SRCS = \
	src/lib6502-core/cpu_engine.cpp \
	src/lib6502-core/cpu_6502.cpp \
	src/lib6502-core/opcodes/6502.cpp \
	src/lib6502-core/opcodes/6502_undoc.cpp \
	src/lib6502-core/opcodes/65c02.cpp \
	src/lib6502-core/opcodes/65ce02.cpp \
	src/lib6502-core/opcodes/45gs02.cpp

# --- lib6502-mem ---
MEM_SRCS = \
	src/lib6502-mem/memory.cpp \
	src/lib6502-mem/interrupts.cpp

# --- lib6502-devices ---
DEV_SRCS = \
	src/lib6502-devices/audio.cpp \
	src/lib6502-devices/device/vic2.cpp \
	src/lib6502-devices/device/vic2_io.cpp \
	src/lib6502-devices/device/sid_io.cpp \
	src/lib6502-devices/device/mega65_io.cpp \
	src/lib6502-devices/device/cia_io.cpp

# --- lib6502-toolchain ---
TOOL_SRCS = \
	src/lib6502-toolchain/symbols.cpp \
	src/lib6502-toolchain/list_parser.cpp \
	src/lib6502-toolchain/disassembler.cpp \
	src/lib6502-toolchain/metadata.cpp \
	src/lib6502-toolchain/patterns.cpp \
	src/lib6502-toolchain/project_manager.cpp

# --- lib6502-debug ---
DBG_SRCS = \
	src/lib6502-debug/condition.cpp \
	src/lib6502-debug/debug_context.cpp

# --- sim_api (front-facing) ---
API_SRCS = src/sim_api.cpp

ALL_LIB_SRCS = $(CORE_SRCS) $(MEM_SRCS) $(DEV_SRCS) $(TOOL_SRCS) $(DBG_SRCS) $(API_SRCS)
ALL_LIB_OBJS = $(ALL_LIB_SRCS:.cpp=.o)

LIB_TARGET = libsim6502.a

all: sim6502 gui

$(LIB_TARGET): $(ALL_LIB_OBJS)
	ar rcs $@ $^

# Library objects compiled with LIB_IFLAGS (sim_api.cpp needs FRONT_IFLAGS)
src/lib6502-core/%.o: src/lib6502-core/%.cpp
	$(CXX) $(CXXFLAGS) $(LIB_IFLAGS) -c -o $@ $<

src/lib6502-core/opcodes/%.o: src/lib6502-core/opcodes/%.cpp
	$(CXX) $(CXXFLAGS) $(LIB_IFLAGS) -c -o $@ $<

src/lib6502-mem/%.o: src/lib6502-mem/%.cpp
	$(CXX) $(CXXFLAGS) $(LIB_IFLAGS) -c -o $@ $<

src/lib6502-devices/%.o: src/lib6502-devices/%.cpp
	$(CXX) $(CXXFLAGS) $(LIB_IFLAGS) -c -o $@ $<

src/lib6502-devices/device/%.o: src/lib6502-devices/device/%.cpp
	$(CXX) $(CXXFLAGS) $(LIB_IFLAGS) -c -o $@ $<

src/lib6502-toolchain/%.o: src/lib6502-toolchain/%.cpp
	$(CXX) $(CXXFLAGS) $(LIB_IFLAGS) -c -o $@ $<

src/lib6502-debug/%.o: src/lib6502-debug/%.cpp
	$(CXX) $(CXXFLAGS) $(LIB_IFLAGS) -c -o $@ $<

# sim_api.cpp needs FRONT_IFLAGS (sees both sim_api.h and all libs)
src/sim_api.o: src/sim_api.cpp
	$(CXX) $(CXXFLAGS) $(FRONT_IFLAGS) -c -o $@ $<

# --- CLI Frontend ---
CLI_COMMANDS_SRCS = \
	src/cli/commands/StepCmd.cpp \
	src/cli/commands/NextCmd.cpp \
	src/cli/commands/FinishCmd.cpp \
	src/cli/commands/HistoryCmd.cpp \
	src/cli/commands/BreakCmd.cpp \
	src/cli/commands/EnvCmd.cpp \
	src/cli/commands/DevicesCmd.cpp \
	src/cli/commands/IdiomsCmd.cpp \
	src/cli/commands/CommandRegistry.cpp

CLI_SRCS = src/cli/main.cpp src/cli/commands.cpp $(CLI_COMMANDS_SRCS)
CLI_OBJS = $(CLI_SRCS:.cpp=.o)
TARGET   = sim6502

$(TARGET): $(CLI_OBJS) $(LIB_TARGET)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SDL2_LIBS)

src/cli/%.o: src/cli/%.cpp
	$(CXX) $(CXXFLAGS) $(FRONT_IFLAGS) -c -o $@ $<

src/cli/commands/%.o: src/cli/commands/%.cpp
	$(CXX) $(CXXFLAGS) $(FRONT_IFLAGS) -c -o $@ $<

# --- GUI Frontend ---
IMGUI_DIR  = src/gui/imgui
IMGUI_BACK = $(IMGUI_DIR)/backends

SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL2_LIBS   := $(shell pkg-config --libs   sdl2 2>/dev/null)
GL_LIBS      = -lGL -lpthread

IMGUI_SRCS = \
	$(IMGUI_DIR)/imgui.cpp \
	$(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_BACK)/imgui_impl_sdl2.cpp \
	$(IMGUI_BACK)/imgui_impl_opengl3.cpp
IMGUI_OBJS = $(IMGUI_SRCS:.cpp=.o)

GUI_TARGET = sim6502-gui

src/gui/main.o: src/gui/main.cpp $(IMGUI_DIR)/imgui.h src/sim_api.h src/gui/imgui_filedlg.h
	$(CXX) $(CXXFLAGS) $(FRONT_IFLAGS) $(SDL2_CFLAGS) -I $(IMGUI_DIR) -I $(IMGUI_BACK) -c -o $@ $<

.PHONY: gui
gui: $(IMGUI_DIR)/imgui.h $(GUI_TARGET)

$(GUI_TARGET): src/gui/main.o $(LIB_TARGET) $(IMGUI_OBJS)
	$(CXX) -o $@ $^ $(SDL2_LIBS) $(GL_LIBS)

$(IMGUI_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	$(CXX) -O2 -I $(IMGUI_DIR) -I $(IMGUI_BACK) -c -o $@ $<

$(IMGUI_BACK)/%.o: $(IMGUI_BACK)/%.cpp
	$(CXX) -O2 $(SDL2_CFLAGS) -I $(IMGUI_DIR) -I $(IMGUI_BACK) -c -o $@ $<

# Auto-fetch ImGui (docking branch) if not present
$(IMGUI_DIR)/imgui.h:
	@echo "--- Dear ImGui not found; fetching docking branch ---"
	git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git $(IMGUI_DIR)

# --- Unit Testing ---
UNIT_TEST_SRCS = tests/unit/test_main.cpp tests/unit/test_cpu_arithmetic.cpp tests/unit/test_cpu_opcodes.cpp tests/unit/test_cpu_45gs02.cpp tests/unit/test_cpu_decode.cpp tests/unit/test_memory.cpp tests/unit/test_toolchain.cpp tests/unit/test_debug.cpp tests/unit/test_sim_api.cpp tests/unit/test_devices.cpp tests/unit/test_integration.cpp tests/unit/test_fuzz.cpp tests/unit/test_cli.cpp tests/unit/test_regression.cpp tests/unit/test_patterns_logic.cpp tests/unit/test_templates_logic.cpp
UNIT_TEST_OBJS = $(UNIT_TEST_SRCS:.cpp=.o)
UNIT_TEST_CLI_OBJS = $(CLI_COMMANDS_SRCS:.cpp=.o)
UNIT_TEST_TARGET = unit-tests

$(UNIT_TEST_TARGET): $(UNIT_TEST_OBJS) $(UNIT_TEST_CLI_OBJS) $(LIB_TARGET)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SDL2_LIBS) $(GL_LIBS)

tests/unit/%.o: tests/unit/%.cpp tests/unit/catch.hpp
	$(CXX) $(CXXFLAGS) $(FRONT_IFLAGS) -I tests/unit -c -o $@ $<

.PHONY: unit-test
unit-test: $(UNIT_TEST_TARGET)
	./$(UNIT_TEST_TARGET)

# --- Housekeeping ---
clean:
	rm -f $(ALL_LIB_OBJS) $(CLI_OBJS) $(TARGET) src/gui/main.o $(IMGUI_OBJS) $(GUI_TARGET) $(LIB_TARGET) $(UNIT_TEST_OBJS) $(UNIT_TEST_TARGET)

test: $(TARGET) $(UNIT_TEST_TARGET)
	./$(UNIT_TEST_TARGET)
	./tools/run_tests.py
	./tools/test_patterns.py

.PHONY: all clean gui test unit-test
