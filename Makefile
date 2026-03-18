CC       = gcc
CXX      = g++
# Disable warnings for strncpy/strncat output truncation (e.g. copying x bytes from a string 
# of length y where x < y). This occurs when the destination buffer may not be null-terminated.
CFLAGS   = -Wall -Wextra -O2 -Wno-stringop-truncation
CXXFLAGS = -pthread -Wall -Wextra -O2 -Wno-stringop-truncation

# Include paths for library code (no src/ prefix — each lib is its own root)
LIB_IFLAGS = \
	-I src \
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

# --- lib6502-commands (Shared CLI logic) ---
CMD_SRCS = \
	src/cli/commands.cpp \
	src/cli/commands/StepCmd.cpp \
	src/cli/commands/NextCmd.cpp \
	src/cli/commands/FinishCmd.cpp \
	src/cli/commands/HistoryCmd.cpp \
	src/cli/commands/BreakCmd.cpp \
	src/cli/commands/EnvCmd.cpp \
	src/cli/commands/DevicesCmd.cpp \
	src/cli/commands/IdiomsCmd.cpp \
	src/cli/commands/HelpCmd.cpp \
	src/cli/commands/CommandRegistry.cpp \

# --- sim_api (front-facing) ---
API_SRCS = src/sim_api.cpp

ALL_LIB_SRCS = $(CORE_SRCS) $(MEM_SRCS) $(DEV_SRCS) $(TOOL_SRCS) $(DBG_SRCS) $(CMD_SRCS) $(API_SRCS)
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
CLI_SRCS = src/cli/main.cpp
CLI_OBJS = $(CLI_SRCS:.cpp=.o)
TARGET   = sim6502

# Optional readline support for Tab-completion in CLI interactive mode.
# Enabled automatically when readline is present; define HAVE_READLINE=0 to disable.
READLINE_LIBS   := $(shell pkg-config --libs readline 2>/dev/null || echo "")
READLINE_CFLAGS := $(shell pkg-config --cflags readline 2>/dev/null || echo "")
ifneq ($(READLINE_LIBS),)
    CXXFLAGS += -DHAVE_READLINE $(READLINE_CFLAGS)
endif

$(TARGET): $(CLI_OBJS) $(LIB_TARGET)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SDL2_LIBS) $(READLINE_LIBS)

src/cli/%.o: src/cli/%.cpp
	$(CXX) $(CXXFLAGS) $(FRONT_IFLAGS) -c -o $@ $<

src/cli/commands/%.o: src/cli/commands/%.cpp
	$(CXX) $(CXXFLAGS) $(FRONT_IFLAGS) -c -o $@ $<

# --- GUI Frontend (wxWidgets) ---
SDL2_LIBS   := $(shell pkg-config --libs   sdl2 2>/dev/null)
GL_LIBS      = -lGL -lpthread

WX_CFLAGS   := $(shell wx-config --cflags)
WX_LIBS     := $(shell wx-config --libs std,aui,gl,stc,propgrid)

GUI_SRCS = \
	src/gui/app.cpp \
	src/gui/main_frame.cpp \
	src/gui/main_frame_menus.cpp \
	src/gui/pane_registers.cpp \
	src/gui/pane_disassembly.cpp \
	src/gui/pane_memory.cpp \
	src/gui/pane_console.cpp \
	src/gui/pane_breakpoints.cpp \
	src/gui/pane_trace.cpp \
	src/gui/pane_stack.cpp \
	src/gui/pane_watches.cpp \
	src/gui/pane_snap_diff.cpp \
	src/gui/pane_iref.cpp \
	src/gui/pane_symbols.cpp \
	src/gui/pane_source.cpp \
	src/gui/pane_profiler.cpp \
	src/gui/pane_test_runner.cpp \
	src/gui/pane_devices.cpp \
	src/gui/pane_patterns.cpp \
	src/gui/dialogs.cpp \
	src/gui/pane_vic_screen.cpp \
	src/gui/pane_vic_char_editor.cpp \
	src/gui/pane_vic_sprites.cpp \
	src/gui/pane_vic_regs.cpp \
	src/gui/pane_sid_debugger.cpp \
	src/gui/pane_audio_mixer.cpp

GUI_OBJS = $(GUI_SRCS:.cpp=.o)
GUI_TARGET = sim6502-gui

.PHONY: gui
gui: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_OBJS) $(LIB_TARGET)
	$(CXX) -o $@ $^ $(WX_LIBS) $(SDL2_LIBS) $(GL_LIBS) $(READLINE_LIBS)

src/gui/%.o: src/gui/%.cpp
	$(CXX) $(CXXFLAGS) $(FRONT_IFLAGS) $(WX_CFLAGS) -c -o $@ $<

# --- Unit Testing ---
UNIT_TEST_SRCS = tests/unit/test_main.cpp tests/unit/test_cpu_arithmetic.cpp tests/unit/test_cpu_opcodes.cpp tests/unit/test_cpu_45gs02.cpp tests/unit/test_cpu_decode.cpp tests/unit/test_memory.cpp tests/unit/test_toolchain.cpp tests/unit/test_debug.cpp tests/unit/test_sim_api.cpp tests/unit/test_devices.cpp tests/unit/test_vic2_sprites.cpp tests/unit/test_integration.cpp tests/unit/test_fuzz.cpp tests/unit/test_cli.cpp tests/unit/test_regression.cpp tests/unit/test_patterns_logic.cpp tests/unit/test_templates_logic.cpp
UNIT_TEST_OBJS = $(UNIT_TEST_SRCS:.cpp=.o)
UNIT_TEST_TARGET = unit-tests

$(UNIT_TEST_TARGET): $(UNIT_TEST_OBJS) $(LIB_TARGET)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SDL2_LIBS) $(GL_LIBS) $(READLINE_LIBS)

tests/unit/%.o: tests/unit/%.cpp tests/unit/catch.hpp
	$(CXX) $(CXXFLAGS) $(FRONT_IFLAGS) -I tests/unit -c -o $@ $<

.PHONY: unit-test
unit-test: $(UNIT_TEST_TARGET)
	./$(UNIT_TEST_TARGET)

# --- Housekeeping ---
clean:
	rm -f $(ALL_LIB_OBJS) $(CLI_OBJS) $(TARGET) $(GUI_OBJS) $(GUI_TARGET) $(LIB_TARGET) $(UNIT_TEST_OBJS) $(UNIT_TEST_TARGET)

test: $(TARGET) $(UNIT_TEST_TARGET)
	./$(UNIT_TEST_TARGET)
	./tools/run_tests.py
	./tools/test_patterns.py

.PHONY: all clean gui test unit-test
