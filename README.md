# 6502 Simulator

A single-pass assembler and executor for 6502 and compatible processors, with an interactive monitor, symbol table support, a Dear ImGui-based graphical debugger, and an MCP server for LLM integration.

For a full walkthrough of all features, see **[doc/tutorial.md](doc/tutorial.md)**

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
10. [File Structure](#file-structure)
11. [Known Limitations](#known-limitations)

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

### Memory Model

- **Sparse 28-bit Space**: Supports up to 256MB of physical memory, allocated on demand in 4KB pages.
- **MAP Translation**: Full implementation of the C65/MEGA65 MAP register logic for virtual-to-physical address mapping.
- **Write Logging**: Tracks memory writes for history undo/redo and GUI highlighting.

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

- **CLI**: GCC (or compatible C99 compiler), GNU Make.
- **GUI**: G++ (C++11 or later), `libsdl2-dev`, `libgl-dev`, `pkg-config`. 
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
  -o, --opcodes <CPU>      List all opcodes for a processor
  --info <MNEMONIC>        Show addressing modes and cycle counts for one opcode

EXECUTION
  -a, --address <ADDR>     Override start address (hex $xxxx, or a label name)

DEBUGGING
  -I, --interactive        Enter interactive monitor (no source file required)
  -b, --break <ADDR>       Set a breakpoint (hex address, e.g. 0x1000 or $1000; can also take an optional condition)
  -t, --trace [FILE]       Enable execution trace; optional output file

MEMORY
  -m, --mem <START:END>    Hex-dump memory range on exit (e.g. 0x0200:0x0300)
  -s, --stats              Show memory write statistics on exit

SYMBOL TABLES
  --symbols <FILE>         Load a custom symbol table file
  --preset <ARCH>          Load a built-in preset: c64 | c128 | mega65 | x16
  --show-symbols           Print the loaded symbol table on exit

INTERRUPTS
  -i, --irq <CYCLES>       Fire an IRQ at the given cycle count
  -n, --nmi <CYCLES>       Fire an NMI at the given cycle count

OTHER
  -h, --help               Show help and exit
```

Programs run from address `$0200` by default unless overridden with `-a`.

Execution stops at a `BRK` instruction, `STP`, a breakpoint, or after 100 000 cycles.

---

## Assembler Syntax

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
| `step [n]` | Execute `n` instructions (default 1). |
| `stepback` / `sb` | Step backward 1 instruction in history. |
| `stepfwd` / `sf` | Step forward (re-execute) in history. |
| `run` | Run until BRK, STP, or a breakpoint |
| `break <addr> [cond]` | Set a breakpoint with an optional condition (e.g., `A == $00 && .Z == 1`) |
| `clear <addr>` | Remove a breakpoint |
| `list` | List all breakpoints |
| `regs` | Show all registers |
| `mem <addr> [len]` | Hex dump memory |
| `write <addr> <val>` | Write one byte to memory |
| `bload "file" [addr]` | Load binary or `.prg` (C64 format) file. |
| `bsave "file" <start> <end>` | Save memory to binary or `.prg` file. |
| `disasm [addr [count]]` | Disassemble memory |
| `asm [addr]` | Enter inline assembler at `addr` |
| `jump <addr>` | Set the Program Counter |
| `set <reg> <val>` | Set a register (A X Y Z B S P PC) |
| `flag <flag> <val>` | Set a status flag (N V B D I Z C) |
| `reset` | Reset CPU |
| `help` | Show command summary |
| `quit` / `exit` | Exit the simulator |

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

`plugin-gemini/server.js` is a Node.js MCP server that exposes the simulator to LLMs, allowing AI assistants to write, debug, and execute 6502 assembly code directly.

---

## File Structure

- `src/core/`: The simulator engine (CPU, memory, assembler, disassembler).
- `src/cli/`: Command-line interface and interactive monitor.
- `src/gui/`: Dear ImGui-based graphical debugger.
- `plugin-gemini/`: MCP server for LLM integration.
- `tests/`: Regression test suite.
- `examples/`: Sample assembly programs.

---

## Known Limitations

- **Assembler**: Simple syntax with no macro support (yet), no complex expressions, and no local labels.
- **Label Resolution**: Only the low byte of a label address is emitted by the `.byte label` pseudo-op.
- **Cycle Counts**: While provided, counts may not be 100% cycle-accurate for all addressing modes and page-crossing penalties in all variants.
- **Memory Allocation**: The 64 KB `memory_t` virtual space is stack-allocated; deep call stacks in the simulator itself may cause issues on resource-constrained platforms.
- **Decimal Mode**: BCD flag behavior matches correct arithmetic output but does not currently emulate NMOS-specific undefined N/V/Z flag quirks.

---

## License

Proprietary — see `LICENSE`. Will move to open source at a future date.

**Last Updated**: 2026-03-06
