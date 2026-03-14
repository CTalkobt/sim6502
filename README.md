# 6502 Simulator

An assembler-based executor for 6502 and compatible processors, with an interactive monitor, symbol table support, a Dear ImGui-based graphical debugger, and an MCP server for LLM integration. Assembly is handled by the embedded [KickAssembler](https://theweb.dk/KickAssembler/) (65CE02/45GS02 fork), enabling full macro support and rich diagnostic output.

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

Assembly is performed by **KickAssembler** (`tools/KickAss65CE02.jar`) — a 65CE02/45GS02-extended fork of the standard KickAssembler v5.24. Java 8+ is required.

Key syntax conventions:
- **Comments**: `//` single-line; `/* */` block.
- **Origin**: `* = $0200`
- **CPU selection**: `.cpu _45gs02`, `.cpu _65ce02`, `.cpu _65c02`, `.cpu _6502` — the simulator reads this directive and auto-selects the matching processor; no `-p` flag is needed.
- **Mnemonics**: lowercase (`lda`, `sta`, `jsr`, …)
- **Pseudo-ops**: `.byte`, `.word`, `.text`, `.align`, `.fill`, `.import`; KickAssembler macros and functions are supported.
- **Simulator pseudo-ops**: Embedded in `//` comments so files assemble cleanly without the simulator; see [Simulator Pseudo-ops](#simulator-pseudo-ops) below.

When the simulator loads a `.asm` file with no corresponding `.prg`, it auto-assembles via KickAssembler and caches the result.

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
- **Assembler**: Java 8+ (for `tools/KickAss65CE02.jar`).
- **GUI**: G++ (C++17 or later), `libsdl2-dev`, `libgl-dev`, `pkg-config`.
  - *Note: Dear ImGui is automatically fetched from GitHub on first `make gui`.*
- **MCP Server**: Node.js 16+ (`cd src/mcp && npm install` before first use).

---

## Quick Start

```bash
# Assemble and run a file
./sim6502 examples/hello.asm

# Choose a processor variant explicitly
./sim6502 -p 45gs02 examples/45gs02_test.asm

# Processor auto-detected from .cpu directive — no -p needed
./sim6502 examples/45gs02_z_register.asm

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

Execution stops when the program executes `RTS` with an empty stack (returning to the simulator), at `STP`, a breakpoint, or after 100 000 cycles. `BRK` now executes fully — pushing state and jumping through the IRQ vector at `$FFFE/$FFFF`.

---

## Assembler Syntax

Source files use **KickAssembler** syntax (65CE02/45GS02 fork). Mnemonics must be **lowercase**.

### CPU Selection and Origin

```asm
.cpu _45gs02        // select processor (at top of file)
* = $0200           // set program counter origin
```

Accepted CPU identifiers: `_6502`, `_65c02`, `_65ce02`, `_45gs02`.

### Comments

```asm
lda #$42        // single-line comment
/* block
   comment */
```

### Equates

```asm
SCREEN  = $0400     // named constant
COLOUR  = $D800
```

### Labels

```asm
loop:               // global label
    dex
    bne loop
```

### Literal Formats

```asm
lda #$FF        // hex
lda #%11111111  // binary
lda #'A'        // character (ASCII 65)
lda #255        // decimal
```

### Pseudo-ops

#### `* = <addr>`
Set the program counter to an absolute address.
```asm
* = $C000
```

#### `.byte <val>[, <val>…]`
Emit one or more bytes. Accepts all literal formats and label names (emits the low byte of the label address).
```asm
.byte $48, $65, $6C, $6C, $6F   // "Hello"
```

#### `.word <val>[, <val>…]`
Emit 16-bit little-endian words. Useful for vectors and jump tables.
```asm
vectors:
    .word reset_handler, irq_handler
```

#### `.text "string"`
Emit raw string bytes with no null terminator.
```asm
message: .text "Hello, World!\n"
```

#### `.align <n>`
Advance the PC to the next multiple of `n`, padding with zero bytes.
```asm
.align $100     // page-align
```

### Simulator Pseudo-ops

Simulator pseudo-ops are embedded as `//` comments so that the `.asm` file can be assembled directly by KickAssembler without errors. When the simulator loads the file it preprocesses these lines into KickAssembler `.print` statements that emit the resolved address to stdout, which the simulator captures to register the pseudo-op at the correct run-time address.

#### `//.inspect "device"`
When execution reaches this address, the simulator displays state for the named device.
```asm
    sta $D401           // write note frequency hi byte
    //.inspect "SID #1" // show SID state at the next instruction
    lda #20
    jsr wait_frames
```

#### `//.trap "label"`
When a `JSR` targets this address, the simulator logs the call and simulates an immediate `RTS` (useful for intercepting Kernal/ROM calls without loading ROM).
```asm
CHROUT = $FFD2
    //.trap "CHROUT"
```

#### `//.cpu "variant"`
Override or explicitly declare the target processor variant through the metadata pipeline. Useful in `.sym_add` companion files or when you want the simulator to select the correct CPU independent of the `-p` flag.
```asm
//.cpu "45gs02"
```
Accepted values: `45gs02`, `65ce02`, `65c02`, `6502`.

**Note:** For `.asm` files, use the native KickAssembler `.cpu _45gs02` directive at the top of the file — the simulator detects it automatically and the preprocessor additionally injects a `SIM_CPU:` marker so the type flows through the metadata pipeline. The `//.cpu` comment form is primarily useful in `.sym_add` companion files alongside pre-assembled `.prg`/`.bin` binaries.

#### `//.machine "type"`
Set the target machine profile (controls which I/O devices are mapped into the address space).
```asm
//.machine "mega65"
```
Accepted values: `mega65`, `x16`, `c64`, `c128`, `raw6502`.

#### `.sym_add` companion files

For programs loaded directly as `.prg` or `.bin` (built by any assembler), a `.sym_add` file with the same base name can carry supplemental annotations. The simulator loads it automatically alongside `.sym` and `.list`.

A `.sym_add` file may contain any of the following formats:

```
; KickAssembler symbol format
.label sid_freq_hi=$0222

; Simulator pseudo-op markers (same format emitted by KickAssembler stdout)
SIM_INSPECT:0222:SID #1
SIM_TRAP:FFD2:CHROUT

; Processor / machine type metadata
SIM_CPU:45gs02
SIM_MACHINE:mega65
```

`SIM_CPU:` and `SIM_MACHINE:` cause the simulator to switch to the named processor variant and machine profile when the file is loaded — identical to the effect of `//.cpu` and `//.machine` pseudo-ops in `.asm` source.

Produce a `.sym_add` from a KickAssembler build by redirecting stdout:
```bash
java -jar tools/KickAss65CE02.jar prog.asm -symbolfile -o prog.prg > prog.sym_add
```

This allows any external toolchain to annotate `.prg`/`.bin` files with `.inspect`, `.trap`, and processor metadata without requiring re-assembly through the simulator.

---

## Interactive Monitor

Enter the monitor with `-I`. Pressing Enter on a blank line executes a single step.

### Manual Instruction Execution

In addition to monitor commands, you can execute individual assembler statements directly by prefixing them with a **dot (`.`)**.

This is useful for modifying state or testing snippets without advancing the Program Counter of the program you are debugging. The simulator assembles the instruction in the background, executes it, and then restores the original PC.

```bash
> .lda #$FF
Executed: lda #$FF  (Registers updated, PC preserved)
REGS A=FF X=00 Y=00 S=01FF P=22 PC=0200 Cycles=12

> .tax
Executed: tax  (Registers updated, PC preserved)
REGS A=FF X=FF Y=00 S=01FF P=22 PC=0200 Cycles=14
```

Registers are automatically displayed after each manual execution.

### Commands

| Command | Description |
|---------|-------------|
| `step [n]` | Execute `n` instructions (default 1) |
| `next` | Step over subroutine calls |
| `finish` | Run until current subroutine returns |
| `stepback` / `sb` | Step backward 1 instruction in history |
| `stepfwd` / `sf` | Step forward (re-execute) in history |
| `run` | Run until top-level RTS, STP, or a breakpoint |
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

### Companion Files

When any program is loaded the simulator looks for side-files sharing the same base name:

| Extension | Contents | Purpose |
|-----------|----------|---------|
| `.sym` | KickAssembler symbol file | Labels and constants |
| `.list` | ACME-format listing | Source-to-address map |
| `.sym_add` | Supplemental annotations | Additional symbols, `SIM_INSPECT:`/`SIM_TRAP:` markers, and `SIM_CPU:`/`SIM_MACHINE:` processor metadata |

`.sym_add` is useful for `.prg`/`.bin` files produced outside the simulator — place it next to the binary and the simulator picks it up automatically on load, applying all processor and machine-type metadata it contains.

### TRAP Symbols

TRAPs simulate Kernal/ROM routines without requiring the actual ROM to be loaded. When a `JSR` to a TRAP address is encountered:
1. Register state is logged.
2. The routine is skipped (return address popped).
3. CPU cycles are updated (standard 6 cycle overhead).

---

## MCP Server

`src/mcp/index.js` is a Node.js MCP server that exposes the simulator to LLMs, allowing AI assistants to write, debug, and execute 6502 assembly code directly.

### MCP Tools

| Tool | Description |
|------|-------------|
| `load_program` | Assemble and load 6502 source code |
| `step_instruction` | Execute N instructions |
| `run_program` | Run until breakpoint / top-level RTS / STP |
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

The engine is split into focused static libraries that are linked into a single `libsim6502.a`:

- `src/lib6502-core/`: CPU engine and instruction sets.
    - `cpu_engine.cpp/h`: Dispatch loop and CPU lifecycle.
    - `cpu_6502.cpp/h`: Register model and addressing-mode helpers.
    - `cpu_state.h`, `cpu_types.h`, `cpu_observer.h`, `machine.h`, `cycles.h`, `dispatch.h`.
    - `opcodes/`: Per-variant opcode handlers (`6502.cpp`, `65c02.cpp`, `65ce02.cpp`, `45gs02.cpp`, `6502_undoc.cpp`).
- `src/lib6502-mem/`: Memory subsystem.
    - `memory.cpp/h`: 64KB + sparse 28-bit far memory, MAP translation, write logging.
    - `io_handler.h`, `io_registry.h`: I/O device registration and dispatch.
    - `interrupts.cpp/h`: IRQ/NMI handling.
- `src/lib6502-devices/`: Hardware device emulation.
    - `audio.cpp/h`: SID audio output.
    - `device/`: VIC-II, SID I/O, CIA, MEGA65 I/O device implementations.
- `src/lib6502-toolchain/`: Source-level tools.
    - `metadata.cpp/h`: `.prg`/`.bin`/`.asm` loading, KickAssembler auto-assembly, simulator pseudo-op preprocessing (`//.inspect`, `//.trap`, `//.cpu`, `//.machine`), native `.cpu`/`.processor` directive injection, `.sym_add` companion file loading, `detect_asm_cpu_type()`.
    - `symbols.cpp/h`: Symbol table (labels, constants, `SYM_INSPECT`, `SYM_TRAP`, `SYM_PROCESSOR`).
    - `list_parser.cpp/h`: Source-map and ACME-list parsing.
    - `disassembler.cpp/h`: Disassembly for all processor variants.
    - `patterns.cpp/h`: Built-in assembly snippet library.
    - `project_manager.cpp/h`: Template-based project scaffolding.
- `src/lib6502-debug/`: Debugging infrastructure.
    - `debug_context.cpp/h`: Execution history, step-back/forward, snapshot/diff.
    - `breakpoints.h`, `condition.cpp/h`: Breakpoint management and expression evaluator.
    - `trace.h`, `debug_types.h`: Trace ring buffer and shared type definitions.
- `src/sim_api.cpp` / `src/sim_api.h`: Public C API consumed by the CLI and GUI.
- `src/cli/`: Command-line interface and interactive monitor.
    - `main.cpp`: CLI entry point.
    - `commands/`: Individual command classes (Command Pattern).
- `src/gui/`: Dear ImGui-based graphical debugger.
- `src/mcp/`: MCP server for LLM integration (`index.js`).
- `tests/`: Regression test suite (KickAssembler `.asm` files; run with `make test`).
- `tools/`: `run_tests.py`, `test_patterns.py`, `KickAss65CE02.jar`, and helpers.
- `examples/`: Sample assembly programs (KickAssembler syntax, all assemble directly).
- `symbols/`: Pre-built symbol tables (C64, C128, MEGA65, X16).

---

## Known Limitations

- **Assembler**: No macro support yet. Complex expressions in operands are not supported beyond single values and symbol references.
- **Cycle Counts**: While provided, counts may not be 100% cycle-accurate for all addressing modes and page-crossing penalties in all variants.
- **Decimal Mode**: BCD flag behavior matches correct arithmetic output but does not currently emulate NMOS-specific undefined N/V/Z flag quirks.
- **CLI processor auto-detection for `.prg`/`.bin`**: The CLI creates the CPU before loading, so `SIM_CPU:` in a `.sym_add` companion file does not take effect at the CLI level — use `-p <variant>` explicitly. The GUI and MCP paths (via `sim_api`) apply companion-file processor metadata correctly.

---

## License

Proprietary — see `LICENSE`. Will move to open source at a future date.

**Last Updated**: 2026-03-14
