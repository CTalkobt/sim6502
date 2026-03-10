# 6502 Simulator

A two-pass assembler and executor for 6502 and compatible processors, with an interactive monitor, symbol table support, a Dear ImGui-based graphical debugger, and an MCP server for LLM integration.

For a full walkthrough of all features, see **[doc/tutorial.md](doc/tutorial.md)**.
For detailed information on the graphical debugger, see **[README-gui.md](README-gui.md)**.

Help with this development by contributing and buy me coffee at: https://kodecoffee.com/i/ctalkobt

---

## Table of Contents

1. [Features](#features)
2. [Graphical Debugger (GUI)](#graphical-debugger-gui)
3. [Building](#building)
4. [Quick Start](#quick-start)
5. [Command-Line Options](#command-line-options)
6. [Assembler Syntax](#assembler-syntax)
7. [Interactive Monitor](#interactive-monitor)
8. [Symbol Tables](#symbol-tables)
9. [MCP Server](#mcp-server)
10. [Project Scaffolding (Environments)](#project-scaffolding-environments)
11. [File Structure](#file-structure)
12. [Known Limitations](#known-limitations)

---

## Features

### Processor Variants

| Flag | Processor | Notes |
|------|-----------|-------|
| `6502` | NMOS 6502 | Standard documented opcodes |
| `6502-undoc` | NMOS 6502 | Includes undocumented/illegal opcodes |
| `65c02` | CMOS 65C02 | WDC extensions (BIT imm, STZ, TRB, TSB, …) |
| `65ce02` | CSG 65CE02 | Adds Z register, B register, 16-bit branches, word ops |
| `45gs02` | MEGA65 45GS02 | Full 32-bit quad instructions, MAP translation, 28-bit flat addressing |

### 45GS02 Advanced Features

- **Quad Instructions**: 32-bit operations (LDQ, STQ, ADDQ, etc.) via the `$42 $42` prefix.
- **Flat Addressing**: Access up to 256MB of memory using 28-bit physical addresses.
- **MAP Translation**: 8KB virtual memory blocks can be mapped to any 20-bit physical offset.
- **Extended Registers**: 8-bit Z and B registers, and a 16-bit Stack Pointer (SP).
- **New Addressing Modes**: Stack-relative indirect indexed `(d,SP),Y` and flat indirect `[d]` or `[d],Z`.

### Assembler

The simulator includes an integrated assembler that runs before execution:

- **Forward label references** resolved in the second pass.
- **All literal formats**: `$FF` hex, `%10101010` binary, `'A'` character, `123` decimal.
- **Detailed Pseudo-ops**: `.processor`, `.org`, `.byte`, `.word`, `.text`, `.align`, `.bin`.

### Debugger / Monitor

- **History / Time Machine**: Step backwards and forwards through execution history. The simulator records register states and memory deltas.
- **Up to 16 simultaneous breakpoints**: Supports complex conditions (e.g., `PC == $1234 && A == $00 && .Z == 1`).
- **Execution trace**: Detailed logs of every instruction, address, register state, and cycle count.
- **Interactive monitor**: Full-featured CLI for inspecting/modifying registers and memory.
- **ROM TRAP intercept**: Simulate Kernal/ROM calls (e.g., CHROUT) without loading real ROM files.
- **Speed throttle**: Configurable run-speed limiter; `1.0` = C64 PAL clock (~985 kHz), `0` = unlimited.

### VIC-II Support (C64 / C128 / MEGA65)

- **Software rasteriser**: All display modes rendered — Standard Char, Multicolour Char, Extended Colour (ECM), Standard Bitmap, Multicolour Bitmap.
- **Sprite rendering**: All 8 hardware sprites with X/Y expansion, multicolour, X-MSB, and correct priority.
- **CLI commands**: `vic2.info`, `vic2.regs`, `vic2.sprites`, `vic2.savescreen`, `vic2.savebitmap`.
- **MCP tools**: `vic2_info`, `vic2_regs`, `vic2_sprites`, `vic2_savescreen`, `vic2_savebitmap`.

### Memory Model

- **Sparse 28-bit Space**: Supports up to 256MB of physical memory, allocated on demand in 4KB pages.
- **MAP Translation**: Full implementation of the C65/MEGA65 MAP register logic for virtual-to-physical address mapping.
- **Write Logging**: Tracks memory writes for history undo/redo and GUI highlighting.

---

## Project Scaffolding (Environments)

The simulator includes a template-based project scaffolding system to instantly bootstrap development environments.

### CLI Commands

| Command | Description |
|---------|-------------|
| `env list` | List all available project templates (found in `templates/`). |
| `env create <id> <name> [dir] [VAR=VAL...]` | Materialize a new project from a template. Supports custom variable overrides. |

### Templates

Templates are defined as JSON files in the `templates/` directory. They define the directory structure, boilerplate assembly code, Makefiles, and default variables.

Built-in templates include:
- `6502-minimal`: Bare-bones 6502 setup.
- `c64-standard`: C64 setup with BASIC stub andKernal loop.
- `mega65-basic`: Standard 45GS02 structure with Makefile.
- `mega65-quad`: Focused on 32-bit quad operations and extended registers.

---

## Graphical Debugger (GUI)

The simulator includes a comprehensive IDE-style debugger built with **Dear ImGui**. It provides a real-time, multi-pane view of the processor state and allows for interactive development.

### Key GUI Panes

- **Register Display**: Live view of all CPU registers (A, X, Y, Z, B, SP, PC) and status flags with diff highlighting.
- **Disassembly View**: Real-time disassembly with breakpoint toggles, cycle counts, and symbol integration.
- **Memory View**: Hex + ASCII dump with "follow PC/SP" modes and highlighting of last-written bytes.
- **CLI Console**: A full-featured interactive terminal mirroring the CLI monitor mode with command history.
- **Source Viewer**: View the original assembly source code alongside the disassembly.
- **Profiler / Heatmap**: Visualise execution frequency and cycle consumption across the entire 64KB memory map.
- **Watch List**: Monitor specific memory addresses and their values in real-time.
- **Instruction Reference**: Built-in searchable database of all opcodes, addressing modes, and cycle counts for the active processor.
- **Symbol Browser**: Search and inspect all loaded labels, variables, and constants.
- **Stack Inspector**: Human-readable view of the hardware stack and annotated frames.
- **Trace Log**: Rolling ring-buffer of recently executed instructions and CPU states.
- **VIC-II Viewer**: Three panes — Screen (live 384×272 rendered frame with all modes and sprites), Sprites (per-sprite thumbnail grid), and Registers (decoded D011–D01A, video addresses, colour swatches).
- **Speed Throttle**: Run menu slider limits execution to a configurable multiple of the C64 PAL clock; setting persists across sessions via ImGui INI.

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| F5 | Run |
| F6 | Pause |
| Esc | Pause (if running) |
| F7 | Step into (1 instruction) |
| F8 | Step over (executes the JSR body; single-steps otherwise) |
| F9 | Toggle breakpoint at current PC |
| Shift+F7 | Step back 1 instruction in history |
| Shift+F8 | Step forward in history |
| Ctrl+R | Reset CPU |
| Ctrl+L | Focus the filename bar / open load dialog |
| Ctrl+G | Go to address (opens a hex-entry popup, scrolls disassembly) |
| Ctrl+F | Focus search in the active pane (Source / Symbol Browser) |
| Ctrl+Shift+F | Focus disassembly address bar |
| `` ` `` (backtick) | Focus the CLI console input |

---

## Building

```bash
make          # build sim6502 (CLI)
make gui      # build sim6502-gui (Graphical Debugger)
make test     # build and run test suite
make clean    # remove object files and binaries
```

### Requirements

- **CLI**: G++ (C++17 or later), GNU Make.
- **GUI**: G++ (C++17 or later), `libsdl2-dev`, `libgl-dev`, `pkg-config`. 
  - *Note: Dear ImGui is automatically fetched from GitHub on first `make gui`.*

---

## Quick Start

```bash
# Assemble and run a file
./sim6502 examples/hello.asm

# Choose a processor variant
./sim6502 -p 45gs02 examples/45gs02_test.asm

# Launch the Graphical Debugger
./sim6502-gui

# Interactive monitor with a source file
./sim6502 -I examples/hello.asm

# Interactive monitor with no source file (blank memory)
./sim6502 -I

# Interactive monitor with a preset symbol table, no file
./sim6502 -I --preset c64

# Enable execution trace
./sim6502 --trace examples/hello.asm
./sim6502 --trace trace.log examples/hello.asm

# Set a breakpoint and view memory on exit
./sim6502 -b 0x0210 -m 0x0200:0x0220 examples/hello.asm

# Load C64 symbols and display them
./sim6502 --preset c64 --show-symbols examples/hello.asm
```

---

## Command-Line Options

```
PROCESSOR SELECTION
  -p, --processor <CPU>    6502 | 6502-undoc | 65c02 | 65ce02 | 45gs02
  -l, --list               List all available processor types

EXECUTION
  -a, --address <ADDR>     Override start address (hex $xxxx, or a label name)

DEBUGGING
  -I, --interactive        Enter interactive monitor (no source file required)
  -b, --break <ADDR>       Set a breakpoint (hex address, e.g. 0x1000 or $1000; can also take an optional condition)
  -t, --trace [FILE]       Enable execution trace; optional output file

MEMORY
  -m, --mem <START:END>    Hex-dump memory range on exit (e.g. 0x0200:0x0300)

SYMBOL TABLES
  --symbols <FILE>         Load a custom symbol table file
  --preset <ARCH>          Load a built-in preset: c64 | c128 | mega65 | x16
  --show-symbols           Print the loaded symbol table on exit

OTHER
  -J, --json               Emit all monitor output as JSON (used by the MCP server)
  -h, --help               Show help and exit
```

Programs run from address `$0200` by default unless overridden with `-a`.

Execution stops at a `BRK` instruction, `STP`, a breakpoint, or after 100 000 cycles.

---

## Assembler Syntax

### Equates

```asm
SCREEN  = $0400     ; define a named constant
COLOUR  = $D800
```

`NAME = VALUE` equates are resolved before instructions are assembled. The value can be any literal format.

### Local Labels

Labels starting with `.` are local and are intended for use within a single routine:

```asm
mul_loop:
    LSR FACTOR_B
    BCC .skip
    ADC FACTOR_A
.skip:
    DEX
    BNE mul_loop
```

Local labels do not need to be unique across the file.

### Literal Formats

```asm
LDA #$FF        ; hex
LDA #%11111111  ; binary
LDA #'A'        ; character (ASCII 65)
LDA #255        ; decimal
```

All four formats work everywhere a value is expected: immediate operands, addresses, and pseudo-op arguments.

### Pseudo-ops

#### `.processor <variant>`
Select the processor before assembly begins. This may appear anywhere in the file and takes effect immediately.
```asm
.processor 45gs02
```
Accepted values: `6502`, `6502 undoc`, `65c02`, `65ce02`, `45gs02`.

#### `.org <addr>`
Set the program counter to an absolute address for following code or data.
```asm
.org $C000
```

#### `.byte <val>[, <val>…]`
Emit one or more bytes. Accepts all literal formats and label names (emits the low byte of the label address).
```asm
.byte $48, 'e', 108, %01101100, 'o'   ; "Hello"
```

#### `.word <val>[, <val>…]`
Emit 16-bit little-endian words. Useful for vectors and jump tables.
```asm
vectors:
    .word reset_handler, irq_handler
```

#### `.text "string"`
Emit raw string bytes. No implicit null terminator is added. Supports escape sequences `\n \r \t \0 \\ \"`.
```asm
message: .text "Hello, World!\n"
```

#### `.align <n>`
Advance the PC to the next multiple of `n`, padding with zero bytes. Useful for page alignment.
```asm
.align 256      ; align to a page boundary
```

#### `.bin "filename"`
Include a raw binary file at the current PC. The file is read in both passes to ensure labels following the binary are correctly resolved.
```asm
sprite_data: .bin "assets/sprite.bin"
```

---

## Interactive Monitor

Enter the monitor with `-I`. Pressing Enter on a blank line executes a single step.

### Commands

| Command | Description |
|---------|-------------|
| `step [n]` | Execute `n` instructions (default 1) |
| `stepback` / `sb` | Step backward 1 instruction in history |
| `stepfwd` / `sf` | Step forward (re-execute) in history |
| `run` | Run until BRK, STP, or a breakpoint |
| `trace [on\|off\|file <path>]` | Enable/disable execution trace, or redirect to a file |
| `break <addr> [cond]` | Set a breakpoint with an optional condition (e.g., `A == $00 && .Z == 1`) |
| `clear <addr>` | Remove a breakpoint |
| `list` | List all active breakpoints |
| `regs` | Show all registers |
| `mem <addr> [len]` | Hex dump memory |
| `write <addr> <val>` | Write one byte to memory |
| `bload "file" [addr]` | Load binary or `.prg` (C64 format) file |
| `bsave "file" <start> <end>` | Save memory range to binary or `.prg` file |
| `disasm [addr [count]]` | Disassemble memory |
| `asm [addr]` | Enter inline assembler at `addr` (blank line exits) |
| `jump <addr>` | Set the Program Counter |
| `set <reg> <val>` | Set a register (A X Y Z B S P PC) |
| `flag <flag> <val>` | Set a status flag (N V B D I Z C) |
| `processor <type>` | Switch processor variant (6502, 65c02, 65ce02, 45gs02) |
| `processors` | List all supported processor types |
| `info <mnemonic>` | Show addressing modes and cycle counts for one opcode |
| `symbols` | List the loaded symbol table |
| `validate <addr> [REG=val…] : [REG=val…]` | Call subroutine at `addr` with given input registers and verify expected output registers |
| `snapshot` | Record the current memory state as a baseline |
| `diff` | Show all bytes that changed since the last `snapshot` |
| `list_patterns` | List all built-in assembly snippet templates |
| `get_pattern <name>` | Print the full source for a snippet (e.g. `mul8_mega65`, `memcopy`) |
| `speed [scale]` | Get or set run speed. `1.0` = C64 PAL (~985 kHz). `0` = unlimited |
| `reset` | Reset CPU |
| `help` | Show command summary |
| `quit` / `exit` | Exit the simulator |

### VIC-II Commands

The interactive monitor includes commands for inspecting and capturing the VIC-II video chip state.

| Command | Description |
|---------|-------------|
| `vic2.info` | Print VIC-II state summary: mode string, D011/D016/D018, bank, screen/charset/bitmap addresses, border and background colours |
| `vic2.regs` | Full register dump — D011/D016/D018 with every decoded bit, D012 raster line (9-bit), D019 interrupt status (IRQ/RST/MBC/MMC/LP), D01A interrupt enable, bank and all video addresses, all five colour registers (D020 border, D021–D024 BG0–BG3) with colour names |
| `vic2.sprites` | Print all 8 sprite states: X/Y, colour, MCM/XE/YE/BG flags, data address |
| `vic2.savescreen [file]` | Render full 384×272 PAL frame (border + display + sprites) to a PPM file (default: `vic2screen.ppm`) |
| `vic2.savebitmap [file]` | Render the 320×200 active display area (no border; sprites included and clipped to active area) to a PPM file — mode-aware: adapts to Standard Char, Multicolour Char, ECM, Standard Bitmap, or Multicolour Bitmap (default: `vic2bitmap.ppm`) |

### Breakpoint Conditions

Breakpoints can be made conditional using standard assembler expression syntax. Supported operators (in order of precedence):
1. Parentheses: `( )`
2. Unary: `~` (bitwise NOT)
3. Shifts: `<<, >>`
4. Bitwise: `&` (AND), `^` (XOR), `|` (OR)
5. Comparisons: `==, !=, <, >, <=, >=`
6. Logical: `&&` (AND), `||` (OR)

Example: `break $C000 (A & $40) != 0 && .Z == 1`

---

## Symbol Tables

### Preset Architectures

Load with `--preset <name>`: `c64`, `c128`, `mega65`, `x16`.

### TRAP Symbols

TRAPs simulate Kernal/ROM routines without requiring the actual ROM to be loaded. When a `JSR` to a TRAP address is encountered:
1. Register state is logged.
2. The routine is skipped (return address popped).
3. CPU cycles are updated (standard 6 cycle overhead).

---

## MCP Server

`mcp-server/server.js` is a Node.js MCP server that exposes the simulator to LLMs, allowing AI assistants to write, debug, and execute 6502 assembly code directly.

### MCP Tools

| Tool | Description |
|------|-------------|
| `load_program` | Assemble and load 6502 source code |
| `step_instruction` | Execute N instructions |
| `run_program` | Run until breakpoint / BRK / STP |
| `trace_run` | Execute N instructions and return a compact per-instruction log (address, disassembly, register state after each step) |
| `read_registers` | Return current CPU register state |
| `read_memory` | Read a range of memory bytes |
| `write_memory` | Write a byte to memory |
| `reset_cpu` | Reset CPU registers |
| `set_processor` | Switch processor variant |
| `list_processors` | List supported processor types |
| `get_opcode_info` | Fetch addressing modes and cycles for a mnemonic |
| `set_breakpoint` | Set a breakpoint at an address |
| `clear_breakpoint` | Remove a breakpoint |
| `list_breakpoints` | List all active breakpoints |
| `assemble` | Inline-assemble code into memory |
| `disassemble` | Disassemble memory at an address |
| `step_back` | Step back one instruction using execution history |
| `step_forward` | Step forward in history |
| `validate_routine` | Run an array of register test-vectors against a subroutine; returns pass/fail per test |
| `snapshot` | Capture a memory baseline; subsequent execution tracks all writes |
| `diff_snapshot` | Show every address that changed since the last snapshot, with before/after values and writer PC |
| `list_patterns` | List all built-in assembly snippet templates grouped by category and processor |
| `get_pattern` | Return a fully documented, parameterised snippet by name (e.g. `mul8_mega65`, `memcopy`) |
| `speed` | Get or set run-speed throttle (`1.0` = C64) |
| `vic2_info` | Print VIC-II state summary (mode, key addresses, colours) |
| `vic2_regs` | Full register dump: D011–D01A decoded, raster line, interrupt flags, bank, all video addresses, all colour registers |
| `vic2_sprites` | Print all 8 sprite states |
| `vic2_savescreen` | Render full 384×272 PAL frame to PPM |
| `vic2_savebitmap` | Render 320×200 active display area to PPM (mode-aware, no border; sprites included) |
| `list_env_templates` | Discover available project environment templates |
| `create_project` | Bootstrap a new project directory structure from a template |

---

## File Structure

- `src/core/`: The simulator engine.
    - `cpu.h`: Base CPU and Memory state classes.
    - `cpu_6502.cpp/h`: Specialized CPU classes and instruction dispatch.
    - `memory.h`: Bus logic, Math coprocessor, and DMA controller.
    - `opcodes/`: Instruction set implementations.
    - `assembler.cpp`, `disassembler.cpp`, `patterns.cpp`.
- `src/cli/`: Command-line interface and interactive monitor.
    - `main.cpp`: CLI entry point.
    - `commands/`: Individual command classes (Command Pattern).
- `src/gui/`: Dear ImGui-based graphical debugger.
- `mcp-server/`: MCP server for LLM integration.
- `tests/`: Regression test suite (`make test`).
- `tools/`: Test scripts.
- `examples/`: Sample assembly programs.
- `symbols/`: Pre-built symbol tables.

---

## Known Limitations

- **Assembler**: No macro support yet. Complex expressions in operands are not supported beyond single values and symbol references.
- **Cycle Counts**: While provided, counts may not be 100% cycle-accurate for all addressing modes and page-crossing penalties in all variants.
- **Decimal Mode**: BCD flag behavior matches correct arithmetic output but does not currently emulate NMOS-specific undefined N/V/Z flag quirks.

---

## License

Proprietary — see `LICENSE`. Will move to open source at a future date.

**Last Updated**: 2026-03-10
