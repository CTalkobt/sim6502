# 6502 Simulator

A multi-processor 6502-family assembler, executor, and interactive debugger with a full IDE-style graphical debugger, an interactive CLI monitor, and an MCP server for LLM integration. Assembly is handled by the embedded [KickAssembler](https://theweb.dk/KickAssembler/) (65CE02/45GS02 fork), enabling full macro support and rich diagnostic output.

Supported processor families range from the original NMOS 6502 up to the MEGA65 45GS02, including undocumented opcodes, the CSG 65CE02 extended register set, and the full 28-bit flat address space and 32-bit quad instruction set of the 45GS02.

For a full walkthrough of all features, see **[doc/tutorial.md](doc/tutorial.md)**.
For detailed information on the graphical debugger, see **[README-gui.md](README-gui.md)**.

Help with this development by contributing and buy me a coffee at: https://kodecoffee.com/i/ctalkobt

---

## Table of Contents

1. [Features Summary](#features-summary)
2. [Building](#building)
3. [Quick Start](#quick-start)
4. [Command-Line Options](#command-line-options)
5. [Processor Variants](#processor-variants)
6. [Assembler Syntax](#assembler-syntax)
7. [Simulator Pseudo-ops](#simulator-pseudo-ops)
8. [Interactive Monitor](#interactive-monitor)
9. [Symbol Tables](#symbol-tables)
10. [VIC-II Support](#vic-ii-support)
11. [MCP Server](#mcp-server)
12. [Project Scaffolding (Environments)](#project-scaffolding-environments)
13. [Graphical Debugger (GUI)](#graphical-debugger-gui)
14. [File Structure](#file-structure)
15. [Known Limitations](#known-limitations)

---

## Features Summary

- **Multi-processor**: 6502, 6502 undocumented, 65C02, 65CE02, and 45GS02 (MEGA65).
- **Assembler**: KickAssembler-based, full macro and pseudo-op support; auto-detects `.cpu` directive.
- **Debugger / Monitor**: Step, step-back (time machine), breakpoints with conditional expressions, execution trace, watch lists, snapshot/diff.
- **VIC-II emulation**: Full software rasteriser for all character and bitmap modes, all 8 hardware sprites, CLI and MCP inspection commands.
- **SID audio**: Basic SID I/O register emulation.
- **Interactive CLI monitor**: Full REPL with history, inline assembler, memory edit/inspect, and all debug commands.
- **Graphical debugger (GUI)**: wxWidgets/wxAUI IDE with 20+ dockable panes, live register/disassembly/memory views, VIC-II visual editors, audio mixer, profiler, and more.
- **MCP server**: Node.js server exposing the full simulator API to LLMs via the Model Context Protocol.
- **Project scaffolding**: Template-based project bootstrapping for 6502, C64, and MEGA65 targets.
- **Snippet library**: 13+ documented assembly snippets (math, memory ops, hardware routines) accessible from CLI, MCP, and GUI.

---

## Building

```bash
make          # build sim6502 (CLI) and sim6502-gui (GUI)
make sim6502  # build CLI only
make gui      # build GUI only
make test     # build and run test suite
make clean    # remove object files and binaries
```

### Requirements

- **CLI**: G++ (C++17 or later), GNU Make.
- **Assembler**: Java 8+ (for `tools/KickAss65CE02.jar`).
- **GUI**: G++ (C++17 or later), `libwxgtk3.2-dev` (or equivalent wxWidgets 3.x package), `libgl-dev`, `pkg-config`.
  - wxWidgets must be built with GL, AUI, STC, and PropGrid support.
- **MCP Server**: Node.js 16+ (`cd src/mcp && npm install` before first use).

---

## Quick Start

```bash
# Assemble and run a file (processor auto-detected from .cpu directive)
./sim6502 examples/hello.asm

# Choose a processor variant explicitly
./sim6502 -p 45gs02 examples/45gs02_test.asm

# Launch the Graphical Debugger
./sim6502-gui

# Load a file directly into the GUI
./sim6502-gui examples/hello.asm

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
  -b, --break <ADDR>       Set a breakpoint (hex address, e.g. 0x1000 or $1000; optional condition)
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

Execution stops when the program executes `RTS` with an empty stack (returning to the simulator), at `STP`, a breakpoint, or after 100 000 cycles. `BRK` executes fully — pushing state and jumping through the IRQ vector at `$FFFE/$FFFF`.

---

## Processor Variants

| Flag | Processor | Notes |
|------|-----------|-------|
| `6502` | NMOS 6502 | Standard documented opcodes |
| `6502-undoc` | NMOS 6502 | Includes undocumented/illegal opcodes |
| `65c02` | CMOS 65C02 | WDC extensions (BIT imm, STZ, TRB, TSB, …) |
| `65ce02` | CSG 65CE02 | Adds Z register, B register, 16-bit branches, word ops |
| `45gs02` | MEGA65 45GS02 | Full 32-bit quad instructions, MAP translation, 28-bit flat addressing |

### 45GS02 Advanced Features

- **Quad Instructions**: 32-bit operations (LDQ, STQ, ADDQ, etc.) via the `$42 $42` prefix.
- **Flat Addressing**: Access up to 256 MB of memory using 28-bit physical addresses.
- **MAP Translation**: 8 KB virtual memory blocks can be mapped to any 20-bit physical offset.
- **Extended Registers**: 8-bit Z and B registers, and a 16-bit Stack Pointer (SP).
- **New Addressing Modes**: Stack-relative indirect indexed `(d,SP),Y` and flat indirect `[d]` or `[d],Z`.

---

## Assembler Syntax

Source files use **KickAssembler** syntax (65CE02/45GS02 fork). Mnemonics must be **lowercase**.

### CPU Selection and Origin

```asm
.cpu _45gs02        // select processor (at top of file)
* = $0200           // set program counter origin
```

Accepted CPU identifiers: `_6502`, `_65c02`, `_65ce02`, `_45gs02`.

The simulator reads the `.cpu` directive and automatically selects the matching processor; no `-p` flag is needed.

### Comments

```asm
lda #$42        // single-line comment
/* block
   comment */
```

### Equates and Labels

```asm
SCREEN  = $0400     // named constant
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

| Directive | Description |
|-----------|-------------|
| `* = <addr>` | Set program counter |
| `.byte <val>[, …]` | Emit bytes |
| `.word <val>[, …]` | Emit 16-bit little-endian words |
| `.text "string"` | Emit raw string bytes (no null terminator) |
| `.align <n>` | Advance PC to next multiple of `n`, pad with zeros |
| `.fill <n>, <val>` | Emit `n` copies of `val` |
| `.import binary "file"` | Inline a binary file |

KickAssembler macros and functions are also fully supported.

---

## Simulator Pseudo-ops

Simulator pseudo-ops are written as `//` comments so the `.asm` file can be assembled directly by KickAssembler without errors. When the simulator loads the file it preprocesses these lines into KickAssembler `.print` statements that emit the resolved address, which the simulator captures to register the pseudo-op at the correct runtime address.

### `//.inspect "device"`

When execution reaches this address, the simulator displays state for the named device.

```asm
    sta $D401               // write note frequency hi byte
    //.inspect "SID #1"     // show SID state here
    lda #20
    jsr wait_frames
```

### `//.trap "label"`

When a `JSR` targets this address, the simulator logs the call and simulates an immediate `RTS` — useful for intercepting Kernal/ROM calls without loading ROM.

```asm
CHROUT = $FFD2
    //.trap "CHROUT"
```

### `//.cpu "variant"`

Override or explicitly declare the target processor variant. Primarily useful in `.sym_add` companion files alongside pre-assembled `.prg`/`.bin` binaries.

```asm
//.cpu "45gs02"
```

Accepted values: `45gs02`, `65ce02`, `65c02`, `6502`.

**Note:** For `.asm` files, use the native KickAssembler `.cpu _45gs02` directive — the simulator detects it automatically and additionally injects a `SIM_CPU:` marker so the type flows through the metadata pipeline.

### `//.machine "type"`

Set the target machine profile, controlling which I/O devices are mapped into the address space.

```asm
//.machine "mega65"
```

Accepted values: `mega65`, `x16`, `c64`, `c128`, `raw6502`.

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

A `.sym_add` file may contain any of the following formats:

```
; KickAssembler symbol format
.label sid_freq_hi=$0222

; Simulator pseudo-op markers
SIM_INSPECT:0222:SID #1
SIM_TRAP:FFD2:CHROUT

; Processor / machine type metadata
SIM_CPU:45gs02
SIM_MACHINE:mega65
```

Produce a `.sym_add` from a KickAssembler build by redirecting stdout:

```bash
java -jar tools/KickAss65CE02.jar prog.asm -symbolfile -o prog.prg > prog.sym_add
```

### TRAP Symbols

TRAPs simulate Kernal/ROM routines without loading the actual ROM. When a `JSR` to a TRAP address is encountered:
1. Register state is logged.
2. The routine body is skipped (return address popped from stack).
3. CPU cycle count is updated (6-cycle JSR overhead).

---

## Interactive Monitor

Enter the monitor with `-I`. Pressing Enter on a blank line executes a single step.

### Manual Instruction Execution

Prefix any assembler statement with a **dot (`.`)** to execute it immediately. The simulator assembles the instruction in the background, executes it, and then restores the original PC — useful for modifying state or testing snippets without affecting the program under debug.

```bash
> .lda #$FF
Executed: lda #$FF
REGS A=FF X=00 Y=00 S=01FF P=22 PC=0200 Cycles=12

> .tax
Executed: tax
REGS A=FF X=FF Y=00 S=01FF P=22 PC=0200 Cycles=14
```

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
| `break <addr> [cond]` | Set a breakpoint with an optional condition |
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
| `validate <addr> [REG=val…] : [REG=val…]` | Call subroutine at `addr` with input registers and verify expected output |
| `snapshot` | Record the current memory state as a baseline |
| `diff` | Show all bytes that changed since the last `snapshot` |
| `list_patterns` | List all built-in assembly snippet templates |
| `get_pattern <name>` | Print the full source for a snippet (e.g. `mul8_mega65`, `memcopy`) |
| `speed [scale]` | Get or set run speed. `1.0` = C64 PAL (~985 kHz). `0` = unlimited |
| `reset` | Reset CPU |
| `help` | Show command summary |
| `quit` / `exit` | Exit the simulator |

### VIC-II Commands

| Command | Description |
|---------|-------------|
| `vic2.info` | Print VIC-II state summary: mode, D011/D016/D018, bank, screen/charset/bitmap addresses, border and background colours |
| `vic2.regs` | Full register dump — D011/D016/D018 decoded bit-by-bit, D012 raster line (9-bit), D019 interrupt status, D01A interrupt enable, bank, all video addresses, all five colour registers with colour names |
| `vic2.sprites` | Print all 8 sprite states: X/Y, colour, MCM/XE/YE/BG flags, data address |
| `vic2.savescreen [file]` | Render full 384×272 PAL frame (border + display + sprites) to a PPM file (default: `vic2screen.ppm`) |
| `vic2.savebitmap [file]` | Render the 320×200 active display area (no border; sprites included and clipped) to a PPM file — mode-aware (default: `vic2bitmap.ppm`) |

### Breakpoint Conditions

Breakpoints can be made conditional using standard expression syntax. Supported operators (in precedence order):

1. Parentheses: `( )`
2. Unary: `~` (bitwise NOT)
3. Shifts: `<<`, `>>`
4. Bitwise: `&`, `^`, `|`
5. Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`
6. Logical: `&&`, `||`

Example: `break $C000 (A & $40) != 0 && .Z == 1`

---

## VIC-II Support

The simulator includes a full software rasteriser for the Commodore VIC-II chip (C64 / C128 / MEGA65).

### Display Modes

All five standard VIC-II display modes are rendered:

- Standard Character Mode
- Multicolour Character Mode
- Extended Colour Mode (ECM)
- Standard Bitmap Mode
- Multicolour Bitmap Mode

### Sprites

All 8 hardware sprites are supported with:
- X/Y positioning and X-MSB wrapping
- X and Y expansion (XE/YE)
- Multicolour mode (two shared colours + per-sprite colour)
- Correct sprite-background and sprite-sprite priority

### Render Outputs

Render functions produce full PPM images for screen captures and toolchain integration. The GUI exposes a live OpenGL-rendered VIC-II screen that updates in real time.

---

## MCP Server

`src/mcp/index.js` is a Node.js MCP server that exposes the simulator to LLMs, allowing AI assistants to write, debug, and execute 6502 assembly code directly.

```bash
cd src/mcp
npm install
node index.js
```

### MCP Tools

| Tool | Description |
|------|-------------|
| `load_program` | Assemble and load 6502 source code |
| `step_instruction` | Execute N instructions |
| `run_program` | Run until breakpoint / top-level RTS / STP |
| `trace_run` | Execute N instructions and return a per-instruction log (address, disassembly, register state) |
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
| `snapshot` | Capture a memory baseline |
| `diff_snapshot` | Show every address that changed since the last snapshot, with before/after values and writer PC |
| `list_patterns` | List all built-in assembly snippet templates grouped by category and processor |
| `get_pattern` | Return a fully documented, parameterised snippet by name (e.g. `mul8_mega65`, `memcopy`) |
| `speed` | Get or set run-speed throttle (`1.0` = C64 PAL) |
| `vic2_info` | Print VIC-II state summary |
| `vic2_regs` | Full VIC-II register dump: D011–D01A decoded, raster line, interrupt flags, bank, all video addresses, all colour registers |
| `vic2_sprites` | Print all 8 sprite states |
| `vic2_savescreen` | Render full 384×272 PAL frame to PPM |
| `vic2_savebitmap` | Render 320×200 active display area to PPM (mode-aware, no border; sprites included) |
| `list_env_templates` | Discover available project environment templates |
| `create_project` | Bootstrap a new project directory structure from a template |

---

## Project Scaffolding (Environments)

The simulator includes a template-based project scaffolding system to instantly bootstrap new development environments.

### CLI Commands

| Command | Description |
|---------|-------------|
| `env list` | List all available project templates |
| `env create <id> <name> [dir] [VAR=VAL...]` | Materialize a new project from a template with optional variable overrides |

### Templates

Templates are JSON files in the `templates/` directory defining the directory structure, boilerplate assembly source, Makefiles, and default variables. Built-in templates:

| Template | Description |
|----------|-------------|
| `6502-minimal` | Bare-bones 6502 setup |
| `c64-standard` | C64 setup with BASIC stub and Kernal loop |
| `mega65-basic` | Standard 45GS02 project structure with Makefile |
| `mega65-quad` | Focused on 32-bit quad operations and extended registers |

---

## Graphical Debugger (GUI)

The simulator includes a comprehensive IDE-style debugger built with **wxWidgets** and **wxAUI**. It provides a real-time, multi-pane view of the processor state and allows for interactive development and deep-dive debugging.

For full details on every pane, keyboard shortcut, and menu option, see **[README-gui.md](README-gui.md)**.

### Launching

```bash
./sim6502-gui               # start with an empty workspace
./sim6502-gui prog.asm      # load a file on startup
```

### Key Capabilities

- **Register View**: Live display of all CPU registers with diff highlighting, inline hex/decimal editing, clickable P-flag bit toggles, and automatic show/hide of Z and B registers based on the active CPU type.
- **Disassembly**: Real-time disassembly with breakpoint gutter, symbol labels, and cycle counts.
- **Memory Views**: Up to 4 independent hex+ASCII/PETSCII dump windows with configurable follow mode (PC/SP/word pointer), inline byte editing, and individually titled views.
- **Console**: Full REPL mirroring the CLI monitor mode with command history.
- **Source View**: Original `.asm` source with current-line highlighting.
- **Profiler/Heatmap**: 256×256 pixel execution-frequency and cycle-consumption map of the 64 KB address space.
- **Breakpoints Pane**: GUI breakpoint management with add/delete/clear-all.
- **Trace Log**: Rolling ring-buffer of recently executed instructions and register states.
- **Stack Inspector**: Annotated view of page 1 ($0100–$01FF) with return-address heuristics.
- **Watch List**: Monitor specific memory addresses in real time.
- **Snapshot Diff**: Take a memory baseline and inspect every byte changed since.
- **Instruction Reference**: Searchable opcode database for the active processor.
- **Symbol Browser**: Search and navigate all loaded labels.
- **Test Runner**: Subroutine validation with register input/output test vectors.
- **Idiom Library**: Browse and copy documented assembly snippets.
- **I/O Devices**: Live property-grid view of mapped hardware register state.
- **VIC-II Screen**: OpenGL-rendered live 384×272 PAL frame with all modes and sprites.
- **VIC-II Character Editor**: Full pixel editor for the 256-character character set with MCM mode, transform tools, copy/paste, undo, and export.
- **VIC-II Sprites**: Pixel editor for all 8 hardware sprites with undo, lock, transforms, and save/load.
- **VIC-II Registers**: Decoded view of all VIC-II control registers.
- **SID Debugger**: Visualise SID register state and voice activity.
- **Audio Mixer**: Per-SID volume slider (0–15) with mute toggle and percentage display; MEGA65 adds stereo panning for all 4 SIDs.

---

## File Structure

```
sim6502/
├── src/
│   ├── lib6502-core/         CPU engine, opcode handlers
│   │   └── opcodes/          Per-variant handlers (6502, 65c02, 65ce02, 45gs02, 6502_undoc)
│   ├── lib6502-mem/          Memory subsystem, MAP translation, I/O dispatch
│   ├── lib6502-devices/      Hardware device emulation (VIC-II, SID, CIA, MEGA65)
│   │   └── device/
│   ├── lib6502-toolchain/    Assembler integration, disassembler, symbols, patterns
│   ├── lib6502-debug/        Breakpoints, conditions, trace, history, snapshot/diff
│   ├── cli/                  CLI entry point and interactive monitor
│   │   └── commands/         Command Pattern classes (StepCmd, BreakCmd, etc.)
│   ├── gui/                  wxWidgets/wxAUI graphical debugger (20+ panes)
│   ├── mcp/                  MCP server for LLM integration (Node.js)
│   ├── sim_api.cpp/.h        Public C API consumed by CLI and GUI
│   └── ...
├── tests/                    Regression test suite (.asm files; run with make test)
├── tools/                    run_tests.py, test_patterns.py, KickAss65CE02.jar
├── examples/                 Sample assembly programs
├── templates/                Project scaffolding templates (JSON)
├── symbols/                  Pre-built symbol tables (C64, C128, MEGA65, X16)
└── doc/                      Tutorial and documentation
```

### Library Details

- **`src/lib6502-core/`**: CPU dispatch loop (`cpu_engine.cpp`), register model and addressing-mode helpers (`cpu_6502.cpp`), shared types (`cpu_state.h`, `cpu_types.h`, `machine.h`, `cycles.h`, `dispatch.h`).
- **`src/lib6502-mem/`**: 64 KB + sparse 28-bit far memory, MAP translation, write logging (`memory.cpp`); IRQ/NMI handling (`interrupts.cpp`); I/O device registration and dispatch (`io_handler.h`, `io_registry.h`).
- **`src/lib6502-devices/`**: SID audio output (`audio.cpp`); VIC-II rasteriser and I/O (`vic2.cpp`, `vic2_io.cpp`); SID I/O (`sid_io.cpp`); CIA (`cia_io.cpp`); MEGA65 extended I/O (`mega65_io.cpp`).
- **`src/lib6502-toolchain/`**: `.prg`/`.bin`/`.asm` loading and KickAssembler auto-assembly with simulator pseudo-op preprocessing (`metadata.cpp`); symbol table (`symbols.cpp`); ACME-list source map (`list_parser.cpp`); disassembler (`disassembler.cpp`); built-in snippet library (`patterns.cpp`); project template scaffolding (`project_manager.cpp`).
- **`src/lib6502-debug/`**: Execution history, step-back/forward, snapshot/diff (`debug_context.cpp`); breakpoint management and expression evaluator (`breakpoints.h`, `condition.cpp`); trace ring buffer and shared type definitions (`trace.h`, `debug_types.h`).

---

## Known Limitations

- **Cycle Counts**: While provided, counts may not be 100% cycle-accurate for all addressing modes and page-crossing penalties in every variant.
- **Decimal Mode**: BCD flag behavior matches correct arithmetic output but does not emulate NMOS-specific undefined N/V/Z flag quirks in all cases.
- **CLI processor auto-detection for `.prg`/`.bin`**: The CLI creates the CPU before loading, so `SIM_CPU:` in a `.sym_add` companion file does not take effect at the CLI level — use `-p <variant>` explicitly. The GUI and MCP paths (via `sim_api`) apply companion-file processor metadata correctly.
- **SID audio**: SID register state is emulated for inspection purposes; full audio synthesis output is not implemented.

---

## License

Proprietary — see `LICENSE`. Will move to open source at a future date.

**Last Updated**: 2026-03-19
