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
| `45gs02` | MEGA65 45GS02 | Full 32-bit quad instructions via `$42 $42` prefix, MAP, flat addressing |
| `GUI` | Graphical IDE | Dear ImGui-based multi-pane debugger with VIC-II/IV support |

### Assembler

The simulator includes a two-pass assembler that runs before execution:

- **Forward label references** resolved in the second pass
- **All literal formats**: `$FF` hex, `%10101010` binary, `'A'` character, `123` decimal
- **Pseudo-ops**: `.processor`, `.org`, `.byte`, `.word`, `.text`, `.align`, `.bin`

### Debugger / Monitor

- Up to 16 simultaneous breakpoints
- Execution trace to stdout or file (every instruction, address, cycle count)
- Interactive monitor: step, run, inspect/modify registers and memory
- Interactive mode can be entered **without a source file** (useful for hand-entry or `bload`)
- ROM **TRAP** intercept: simulate Kernal/ROM calls without loading real ROM

### Symbol Tables

- Custom `.sym` files (address, name, type, comment)
- Built-in presets: **C64**, **C128**, **MEGA65**, **Commander X16**
- Symbol types: `LABEL`, `VAR`, `CONST`, `FUNC`, `IO`, `REGION`, `TRAP`

---

## Graphical Debugger (GUI)

Note: The GUI is currently in active development and likely does not support all the functionalities of the full interactive mode at the moment. 

The simulator includes a comprehensive IDE-style debugger built with **Dear ImGui**. It provides a real-time, multi-pane view of the processor state and allows for interactive development.

### Key GUI Panes

- **Register Display**: Live view of all CPU registers (A, X, Y, Z, B, SP, PC) and status flags.
- **Disassembly View**: Real-time disassembly of memory around the program counter, with breakpoint toggles and source-code interleaving.
- **Memory View**: Hex + ASCII dump of any 64KB (or 28-bit) memory region with "follow PC" mode.
- **CLI Console**: A full-featured interactive terminal mirroring the `-I` monitor mode.
- **VIC-II/IV Viewer**: Graphical rendering of C64/MEGA65 video memory, including character modes, bitmaps, and hardware sprites.
- **Execution History**: A "time machine" that records past instructions, allowing you to step backwards and forwards through time to find bugs.
- **Remote Hardware**: Connect the GUI to a real **MEGA65** or **VICE** instance to debug real hardware live.

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

### Labels

```asm
loop:
    DEX
    BNE loop        ; backward and forward references both work

    JSR my_routine

my_routine:
    RTS
```

Labels followed by a colon may also appear on the same line as a pseudo-op:

```asm
data: .byte 1, 2, 3
```

### Pseudo-ops

#### `.processor <variant>`
Select the processor before assembly begins.
```asm
.processor 45gs02
```
Accepted values: `6502`, `6502 undoc`, `65c02`, `65ce02`, `45gs02`.

#### `.org <addr>`
Set the program counter to an absolute address.
```asm
.org $C000
```

#### `.byte <val>[, <val>…]`
Emit one byte per value. Accepts all literal formats and label names (emits the low byte of the label address).
```asm
.byte $48, 'e', 108, %01101100, 'o'   ; H e l l o
```

#### `.word <val>[, <val>…]`
Emit 16-bit little-endian words. Accepts all literal formats and label names.
```asm
vectors:
    .word reset_handler, irq_handler
```

#### `.text "string"`
Emit raw string bytes. No implicit null terminator. Supports escape sequences `\n \r \t \0 \\ \"`.
```asm
message: .text "Hello, World!\n"
```

#### `.align <n>`
Advance the PC to the next multiple of `n`, padding with zero bytes.
```asm
.align 256      ; align to a page boundary
```

#### `.bin "filename"`
Include a raw binary file at the current PC. The file is opened relative to the working directory. The assembler reads the file in both passes so that the PC is advanced correctly for forward-reference resolution.
```asm
sprite_data: .bin "assets/sprite.bin"
```

### Processor Directive

`.processor` may appear anywhere in the file and takes effect immediately during assembly. It is most useful on the first line:

```asm
.processor 45gs02
.org $2000

    LDQ $10         ; 45GS02 quad load
    BRK
```

---

## Interactive Monitor

Enter the monitor with `-I`. A source file is optional — you can start from blank memory and use `bload` or `write` to load code manually.

```bash
./sim6502 -I                        # blank memory
./sim6502 -I program.asm            # pre-load and assemble a file
./sim6502 -I -p 45gs02 --preset mega65   # processor + symbols, no file
```

**Pressing Enter on a blank line executes a single step** (equivalent to `step`).

### Commands

All commands that take an address or numeric value accept `$hex`, `%binary`, or plain decimal.

| Command | Description |
|---------|-------------|
| `step [n]` | Execute `n` instructions (default 1). Blank line also steps once. |
| `run` | Run until BRK, STP, or a breakpoint |
| `break <addr> [cond]` | Set a breakpoint with an optional condition (e.g., `PC == $1234 && A == $00 && .Z == 1`) |
| `clear <addr>` | Remove a breakpoint |
| `list` | List all breakpoints |
| `regs` | Show all registers (A X Y Z B S P PC Cycles) |
| `mem <addr> [len]` | Hex dump starting at address (default 16 bytes) |
| `write <addr> <val>` | Write one byte to memory |
| `bload "file" <addr>` | Load a raw binary file into memory at address |
| `disasm [addr [count]]` | Disassemble `count` instructions from `addr` (defaults: current PC, 15). Unknown bytes shown as `.byte $XX`. Branch targets shown as resolved absolute addresses. |
| `asm [addr]` | Enter inline assembler at `addr` (default: current PC); exit with `.` alone on a line |
| `jump <addr>` | Set the Program Counter |
| `set <reg> <val>` | Set a register (A X Y Z B S P PC) |
| `flag <flag> <val>` | Set a flag (N V B D I Z C) |
| `reset` | Reset CPU to program start address |
| `processor <type>` | Switch active processor type |
| `processors` | List all processor types |
| `info <mnemonic>` | Show addressing modes and cycles for an opcode |
| `help` | Show command summary |
| `quit` / `exit` | Exit the simulator |

### Breakpoint Conditions

Conditions use the syntax `REG OP VAL` or `REG OP REG`, joined by `&&`.
- **Registers**: `A, X, Y, Z, B, S (or SP), P, PC`
- **Flags**: `.C, .Z, .I, .D, .B, .V, .N` (return 1 if set, 0 if clear)
- **Operators**: `==, !=, <, >, <=, >=`
- **Values**: `$hex`, `%binary`, or decimal.

Example: `break $C000 A == $00 && .Z == 1`

### Example Session

```
$ ./sim6502 -I
6502 Simulator Interactive Mode
Type 'help' for commands.
> bload "tests/data/three_bytes.bin" $0200
Loaded 3 bytes at $0200
> regs
REGS A=00 X=00 Y=00 S=FF P=00 PC=0200 Cycles=0
> disasm
$0200: 42                    .byte $42
$0201: AB                    .byte $AB
$0202: FF                    .byte $FF
$0203: 00                 BRK
...
>              <- blank line: single step
STOP $0201
>              <- another step
STOP $0202
> regs
REGS A=00 X=00 Y=00 S=FF P=00 PC=0202 Cycles=0
> mem $0200 4
0200: 42 AB FF 00
> quit
```

---

## Symbol Tables

### Preset Architectures

Load with `--preset <name>`:

| Preset | Coverage |
|--------|----------|
| `c64` | VIC-II, SID, CIA 1/2, full Kernal jump table (as TRAPs) |
| `c128` | All C64 symbols plus MMU, VDC, second SID, extended Kernal |
| `mega65` | 45GS02, Hypervisor, HyperRAM, MEGA65 I/O |
| `x16` | VERA, PSG, SPI, RTC, GPIO, Commander X16 Kernal |

### Custom Symbol Files

```
; Format: ADDRESS  NAME  TYPE  [COMMENT]
; Types: LABEL VAR CONST FUNC IO REGION TRAP

0200  main        LABEL  Program entry point
1000  data_buf    VAR    256-byte scratch buffer
d000  vic_base    IO     VIC-II register base
ffd2  CHROUT      TRAP   Output char in A to current channel
ffe4  GETIN       TRAP   Get char from keyboard -> A
```

Load with `--symbols myfile.sym`.

### TRAP Symbols

When the CPU executes `JSR` to a TRAP address the simulator:

1. Prints the register state at the moment of the call
2. Simulates `RTS` (pops the JSR return address from the stack)
3. Adds 6 cycles

This lets you test programs that call Kernal/ROM routines without loading actual ROM. If a TRAP address is reached by `JMP` or fall-through instead of `JSR`, the simulator halts with a diagnostic.

```
[TRAP] CHROUT               $FFD2  A=48 X=00 Y=00 S=FD P=00  ; Output char A to current channel
[TRAP] CHROUT               $FFD2  A=65 X=00 Y=00 S=FD P=00  ; Output char A to current channel
```

---

## MCP Server

`plugin-gemini/server.js` is a Node.js MCP server that exposes the simulator to LLMs.

### Setup

```bash
make                        # build sim6502 first
cd plugin-gemini
npm install
node server.js
```

### Claude Code Integration

```bash
# Add via CLI
claude mcp add 6502-simulator node /path/to/plugin-gemini/server.js
```

Or add to `~/.claude/settings.json`:

```json
{
  "mcpServers": {
    "6502-simulator": {
      "command": "node",
      "args": ["/path/to/6502-simulator/plugin-gemini/server.js"]
    }
  }
}
```

### Available MCP Tools

| Tool | Description |
|------|-------------|
| `load_program(code)` | Assemble and load a source string |
| `step_instruction(count)` | Execute `count` instructions |
| `read_registers()` | Return all registers |
| `read_memory(address, length)` | Hex dump |
| `write_memory(address, value)` | Write one byte |
| `reset_cpu()` | Reset to initial state |
| `run_program()` | Run until BRK / STP / breakpoint |
| `assemble(code, address?)` | Inline-assemble source lines into memory at `address` (default: PC); returns per-line byte output |
| `disassemble(address?, count?)` | Disassemble `count` instructions from `address` (defaults: PC, 15) |
| `set_breakpoint(address)` | Add a breakpoint |
| `clear_breakpoint(address)` | Remove a breakpoint |
| `list_breakpoints()` | List all breakpoints |
| `list_processors()` | List processor variants |
| `set_processor(type)` | Switch processor |
| `get_opcode_info(mnemonic)` | Addressing modes and cycle counts |

---

## File Structure

```
6502-simulator/
├── src/
│   ├── cli/                    Command-line interface (main.c, commands.c)
│   ├── core/                   Simulator engine and common components
│   │   ├── assembler.c         Single-pass assembler
│   │   ├── cpu_engine.c        CPU execution loop
│   │   ├── disassembler.c      Instruction decoder
│   │   ├── memory.h            Sparse memory and MMU/MAP logic
│   │   ├── sim_api.c           Core API for frontend/IDE integration
│   │   └── opcodes/             Processor-specific opcode tables
│   │       ├── 6502.c          Standard NMOS 6502
│   │       ├── 65c02.c         CMOS 65C02
│   │       ├── 65ce02.c        CSG 65CE02
│   │       └── 45gs02.c        MEGA65 45GS02
│   └── gui/                    Dear ImGui-based debugger source
├── doc/                        Tutorials and development plans
├── ref/                        Reference documentation and PDF manuals
├── symbols/                    Preset architecture symbol tables (.sym)
├── examples/                   Sample assembly programs
├── tests/                      Regression tests (.asm + expectation)
├── tools/                      Test runner and utility scripts
├── plugin-gemini/              MCP server for LLM integration (Node.js)
├── Makefile
└── README.md
```

---

## Test Format

Every test file starts with an expectation comment:

```asm
; EXPECT: A=42 X=10 Y=20 S=FF PC=0206
; PROCESSOR: 45gs02        <- optional, defaults to 6502
```

`make test` runs `tools/run_tests.py`, which compares the `Registers:` line from the simulator's stdout against the expectation.

---

## Known Limitations

- Assembler syntax is simple (no macros, no expressions, no local labels)
- Only the low byte of a label address is emitted by `.byte label`
- Cycle counts are not accurate for all addressing modes and all processor variants
- The 64 KB `memory_t` struct is stack-allocated; very deep call stacks may be an issue on some platforms
- Decimal mode (BCD) flag behaviour matches correct output but does not emulate NMOS undefined N/V/Z quirks

---

## License

Proprietary — see `LICENSE`. Will move to open source at a future date.

**Last Updated**: 2026-02-28
