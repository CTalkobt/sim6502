# 6502 Simulator - Professional 6502 Development & Debugging Platform

A complete, feature-rich simulator for 6502 and compatible processors (65C02, 65CE02, 45GS02) with professional debugging tools, symbol table support, and predefined memory maps for Commodore systems.

Help with this development by contributing and buy me coffee at : https://kodecoffee.com/i/ctalkobt

## Features

### Core Simulation
- ✅ **5 Processor Variants**: 6502, 6502 (undocumented), 65C02, 65CE02, 45GS02
- ✅ **240+ Opcodes**: Complete instruction set support
- ✅ **Accurate Cycle Counting**: Real cycle-accurate timing
- ✅ **Full Memory Space**: 64KB addressable memory
- ✅ **Flag Management**: All processor flags (N, V, B, D, I, Z, C)
- ✅ **Self-Modifying Code**: Full support for programs that modify their own instructions at runtime

### Debugging Features (Phase 1)
- ✅ **Breakpoints**: Set up to 16 breakpoints per run
- ✅ **Execution Trace**: Log every instruction with cycle count
- ✅ **Register Display**: View all registers and flags
- ✅ **Memory Tracking**: Monitor all memory writes

### Analysis Features (Phase 2)
- ✅ **Memory Inspector**: View memory ranges in hex + ASCII
- ✅ **Memory Statistics**: Analyze memory usage
- ✅ **Interrupt Handling**: Simulate IRQ, NMI, BRK interrupts
- ✅ **Stack Analysis**: Monitor stack operations

### Symbol Table Support (Phase 3) ⭐ NEW
- ✅ **Custom Symbol Tables**: Load from text files
- ✅ **Preset Architectures**: C64, C128, Mega65, Commander X16
- ✅ **Symbol Display**: View all loaded symbols with descriptions
- ✅ **Multiple Symbol Types**: Labels, Variables, Functions, I/O Ports, Memory Regions, Traps
- ✅ **ROM Trapping**: Intercept JSR calls to ROM addresses — dumps registers and simulates RTS without needing real ROM code

## Model Context Protocol (MCP) Server ⭐ NEW

The simulator includes a built-in MCP server that allows Large Language Models (LLMs) to interact directly with the 6502 simulation.

### Features
- **Code Loading**: Dynamically load and compile assembly code.
- **Step Execution**: Execute instructions one by one or in batches.
- **State Inspection**: Read registers and memory ranges.
- **State Modification**: Write directly to memory.
- **CPU Control**: Reset the simulation state.

### Installation & Usage

1. **Build the Simulator**:
   ```bash
   make
   ```

2. **Setup the MCP Plugin**:
   ```bash
   cd plugin-gemini
   npm install
   ```

3. **Run the Server**:
   ```bash
   node server.js
   ```

### Configuration for Gemini CLI

The easiest way to add the simulator to Gemini CLI is using the `mcp` command:

```bash
gemini mcp add 6502-simulator node $(pwd)/plugin-gemini/server.js
```

Alternatively, you can manually add it to your Gemini CLI configuration (`.gemini/settings.json` or `~/.gemini/settings.json`):

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

### MCP Tools
The server exposes the following tools:
- `load_program(code)`: Compile and load ASM into the simulator.
- `step_instruction(count)`: Execute `count` instructions.
- `read_registers()`: Returns A, X, Y, SP, PC, and Flags.
- `read_memory(address, length)`: Hex dump of memory range.
- `write_memory(address, value)`: Write byte to memory.
- `reset_cpu()`: Reset to initial state.
- `run_program()`: Run until a breakpoint, BRK, or STP instruction.
- `set_breakpoint(address)`: Set a breakpoint at the given address.
- `clear_breakpoint(address)`: Remove a breakpoint.
- `list_breakpoints()`: List all active breakpoints.
- `list_processors()`: List all supported processor types.
- `set_processor(type)`: Switch processor architecture (e.g. `45gs02`).
- `get_opcode_info(mnemonic)`: Show addressing modes and cycles for an opcode.

## Installation

### Build from Source

```bash
# Extract archive
tar -xzf 6502-simulator.tar.gz
cd 6502-simulator

# Build
make

# Run
./sim6502 examples/hello.asm
```

### Requirements
- GCC or compatible C compiler
- Make
- Linux/macOS/Windows (with appropriate build tools)

## Quick Start

### Basic Execution
```bash
./sim6502 program.asm
```

### With Debugging
```bash
./sim6502 -b 0x1000 -t trace.log program.asm
```

### With Memory View
```bash
./sim6502 -m 0x1000:0x1100 program.asm
```

### With Symbol Table (C64)
```bash
./sim6502 --preset c64 --show-symbols program.asm
```

### With Interrupts
```bash
./sim6502 -i 5000 -n 3000 program.asm
```

### Full Debug Session
```bash
./sim6502 \
  -b 0x2000 \
  -i 1000 \
  -m 0x1000:0x2000 \
  -t trace.log \
  --preset c64 \
  --show-symbols \
  program.asm
```

### Interactive Debugging
```bash
./sim6502 -I program.asm
```

## Symbol Tables

### Preset Architectures

The simulator includes predefined symbol tables for Commodore systems:

#### Commodore 64 (`--preset c64`)
- **VIC-II**: Video Controller (sprites, colors, control registers)
- **SID**: Sound Interface Device (all 3 voices)
- **CIA 1**: Keyboard and Joystick controller
- **CIA 2**: Serial and NMI controller
- **Kernal**: BIOS routines (CHROUT, CHRIN, LOAD, SAVE, etc.)
- **BASIC**: ROM area
- **Memory Regions**: Screen, color, stack

#### Commodore 128 (`--preset c128`)
- **All C64 symbols** (extended)
- **MMU**: Memory Management Unit
- **VDC**: Video Display Controller (80-column mode)
- **Second SID**: Stereo sound support
- **Banking**: Multiple RAM banks (64KB+)
- **Extended Kernal**: C128-specific routines

#### Mega65 (`--preset mega65`)
- **45GS02 CPU**: Extended processor with Z register
- **Hypervisor**: System management interface
- **HyperRAM**: Extended memory controller
- **VERA**: Modern graphics engine (partial)
- **Bitplanes**: Extended video modes
- **Fast I/O**: High-speed peripheral access

#### Commander X16 (`--preset x16`)
- **VERA**: Full Video Retro Architecture
- **Layers**: Sprite engine, tilemap layers
- **SPI**: SD card interface
- **RTC**: Real-time clock
- **PSG**: Programmable Sound Generator
- **GPIO**: General Purpose I/O
- **Modern I/O**: All X16 peripherals

### Custom Symbol Tables

Create your own symbol table file (format: ADDRESS NAME TYPE COMMENT):

```
; My Symbol Table
; Format: ADDRESS NAME TYPE COMMENT
; Types: LABEL, VAR, CONST, FUNC, IO, REGION, TRAP

1000 my_routine FUNC My custom function
2000 my_data VAR My data area
d000 vic_base IO VIC controller base
ffd2 CHROUT TRAP Output char A to current channel
```

Load with:
```bash
./sim6502 --symbols mysymbols.sym program.asm
```

### TRAP — Intercepting ROM Routines

The `TRAP` symbol type lets you write programs that call ROM addresses (e.g. C64 Kernal routines) without needing actual ROM code loaded. When the PC reaches a TRAPped address, the simulator:

1. Prints a register dump showing the CPU state at the point of call
2. Simulates an `RTS` — pops the return address left by the caller's `JSR` and resumes execution there
3. Adds 6 cycles (nominal RTS cost)

If the trap is reached via a `JMP` or fall-through rather than `JSR` (stack return address is `$0000`), execution halts with a diagnostic message.

#### Example output

```
[TRAP] CHROUT               $FFD2  A=41 X=00 Y=00 S=FD P=00  ; Output char A to current channel
[TRAP] CHROUT               $FFD2  A=42 X=00 Y=00 S=FD P=00  ; Output char A to current channel
```

Each line shows the name, address, and full register state at the moment `JSR` transferred control — making it easy to see exactly what arguments your code passed to a ROM routine.

#### Defining TRAPs in a symbol file

```
; Stub out two C64 Kernal routines
ffd2 CHROUT TRAP Output char A to current channel
ffe4 GETIN  TRAP Get char from keyboard queue -> A
```

The preset `--preset c64` (and `c128`, `mega65`) ship with the full Kernal jump table already defined as TRAPs, so any program that calls standard routines will produce readable intercept output without real ROM.

## Command-Line Options

### Processor Selection
```
-p, --processor <CPU>    6502, 6502-undoc, 65c02, 65ce02, 45gs02
-l, --list              List available processors
-o, --opcodes <CPU>     List opcodes for processor
```

### Debugging
```
-b, --break <ADDR>      Set breakpoint (hex address)
-t, --trace [FILE]      Enable trace (optional: to file)
```

### Memory
```
-m, --mem <RANGE>       View memory (0x1000:0x1100)
-s, --stats             Show memory statistics
```

### Symbol Tables
```
--symbols <FILE>        Load custom symbol table
--preset <arch>         Load preset: c64, c128, mega65, x16
--show-symbols          Display loaded symbol table
```

### Interrupts
```
-i, --irq <CYCLES>      Trigger IRQ at cycle count
-n, --nmi <CYCLES>      Trigger NMI at cycle count
```

### Other
```
-I, --interactive       Start in interactive debug mode
-h, --help              Show help message
```

## Interactive Mode

The simulator provides an interactive debug shell for stepping through code and inspecting state.

### Commands
- `step [n]`         : Execute `n` instructions (default: 1)
- `run`              : Execute instructions until a breakpoint or stop condition is met
- `break <addr>`     : Set a breakpoint at address `addr`
- `clear <addr>`     : Clear a breakpoint at address `addr`
- `list`             : List all active breakpoints
- `regs`             : Show all CPU registers and flags (shows Z and B for 45GS02)
- `mem <addr> [len]` : Dump memory hex starting at `addr`
- `write <addr> <val>`: Write byte `val` to memory `addr`
- `jump <addr>`      : Set Program Counter (PC) to address `addr`
- `set <reg> <val>`  : Set register `reg` (A, X, Y, Z, B, S, P, PC) to `val`
- `flag <flag> <0|1>`: Set flag `flag` (C, Z, I, D, B, V, N) to 0 or 1
- `reset`            : Reset CPU to program start address
- `processors`       : List all supported processor types
- `processor <type>` : Change active processor type (e.g., `65c02`)
- `info <mnemonic>`  : Show addressing modes and cycles for an opcode
- `help`             : Show command help
- `quit` / `exit`    : Exit the simulator

### Usage Example
```bash
$ ./sim6502 -I examples/hello.asm
6502 Simulator Interactive Mode
Type 'help' for commands.
> regs
REGS A=00 X=00 Y=00 S=FF P=00 PC=0200 Cycles=0
FLAGS N=0 V=0 B=0 D=0 I=0 Z=0 C=0
> step
STOP 0202
> regs
REGS A=41 X=00 Y=00 S=FF P=00 PC=0202 Cycles=2
...
```

## Output Examples

### Execution Summary
```
6502 Simulator - 6502
═════════════════════════════════════════════════
Registers:
  A: 0x42     X: 0x00     Y: 0x00     S: 0xFF
  PC: 0x0005   P: 0x00

Processor Flags: N=0 V=0 B=0 D=0 I=0 Z=0 C=0

Execution Statistics:
  Instructions: 2
  Cycles: 6
  Avg: 3.0 cycles/instr

Memory Writes: 1
  [0x1000] = 0x42
```

### Memory Dump
```
Memory Dump: 0x1000 to 0x1010
═════════════════════════════════════════════════════════
Address  | Hex Values                    | ASCII
1000     | 41 42 43 44 45 46 47 48 | ABCDEFGH
```

### Symbol Table
```
╔════════════════════════════════════════════════════════╗
║  Symbol Table: symbols/c64.sym                         ║
╠════════════════════════════════════════════════════════╣
║ Address  | Name                | Type          | Comment ║
║ $D000    │ vic_sprite_0_x      │ I/O Port      │ VIC Sprite 0 X ║
║ $D001    │ vic_sprite_0_y      │ I/O Port      │ VIC Sprite 0 Y ║
...
```

## Example Programs

### hello.asm
```asm
.processor 6502
LDA #$41    ; Load 'A'
STA $1000   ; Store at 0x1000
LDA #$42    ; Load 'B'
STA $1001   ; Store at 0x1001
```

Run:
```bash
./sim6502 -m 0x1000:0x1010 hello.asm
```

Output shows memory containing 0x41, 0x42 (ASCII 'A', 'B').

## Architecture Memory Maps

### Commodore 64
```
$0000-$00FF : Zero Page
$0100-$01FF : Stack
$0200-$03FF : Input Buffer / Workspace
$0400-$07FF : Screen Memory
$0800-$9FFF : BASIC Program Space
$A000-$BFFF : BASIC ROM
$C000-$CFFF : BASIC Continuation
$D000-$DFFF : I/O (VIC, SID, CIA, etc.)
$E000-$FFFF : Kernal ROM
```

### Commander X16
```
$0000-$00FF : Zero Page
$0100-$01FF : Stack
$0200-$03FF : Input Buffer
$0400-$07FF : Screen Memory (C64 compat)
$0800-$9EFF : BASIC Program Space
$9F00-$9FFF : I/O Ports (VERA, SPI, RTC, etc.)
$A000-$BFFF : BASIC ROM
$C000-$CFFF : BASIC Extensions
$D000-$DFFF : Reserved
$E000-$FFFF : Kernal ROM
$A0000-$BFFFF : VERA Video RAM (128KB)
```

## Performance

- **Speed**: 1000s of instructions per second
- **Accuracy**: Cycle-exact timing
- **Memory**: < 1 MB runtime
- **Overhead**: Negligible (<1%) with all features enabled

## Documentation

- **QUICK_START.md** - 5-minute quick start guide
- **FINAL_SUMMARY.md** - Complete feature overview
- **PHASE2_COMPLETE_GUIDE.md** - Memory and interrupt details
- **DEBUGGING_GUIDE.md** - Phase 1 debugging features
- **MEMORY_INTERRUPT_GUIDE.md** - Phase 2 analysis features

## File Structure

```
6502-simulator/
├── src/
│   ├── cpu.h              CPU definitions
│   ├── memory.h           Memory management
│   ├── opcodes.h          Opcode definitions
│   ├── breakpoint.h       Breakpoint system
│   ├── trace.h            Execution trace
│   ├── memory_viewer.h    Memory inspector
│   ├── interrupt_manager.h Interrupt handler
│   ├── symbol_table.h     Symbol table support
│   ├── sim6502.c          Main simulator
│   └── opcodes_*.c        Processor implementations
├── symbols/
│   ├── c64.sym            Commodore 64 symbols
│   ├── c128.sym           Commodore 128 symbols
│   ├── mega65.sym         Mega65 symbols
│   └── x16.sym            Commander X16 symbols
├── examples/
│   ├── hello.asm          Hello world example
│   ├── interrupt_demo.asm Interrupt test
│   └── ... more examples
├── README.md              This file
├── Makefile               Build script
└── LICENSE
```

## Creating Your Own Symbol Tables

### Symbol File Format
```
; Comment line starts with ; or #
; Format: ADDRESS NAME TYPE [COMMENT]
; Types: LABEL, VAR, CONST, FUNC, IO, REGION, TRAP

1000 main_loop LABEL Main program loop
2000 data_area VAR Data storage area
d000 vic_base IO VIC-II controller base
fffa nmi_vector REGION NMI interrupt vector
ffd2 CHROUT TRAP Output char A to current channel
```

`TRAP` intercepts any `JSR` to that address: the simulator prints the current register state and simulates an `RTS`, resuming the caller. No ROM code is needed. See the [TRAP section](#trap--intercepting-rom-routines) above for details.

### Loading Symbol Tables
```bash
# Custom symbols
./sim6502 --symbols myarch.sym program.asm

# Preset architectures
./sim6502 --preset c64 program.asm
./sim6502 --preset x16 program.asm

# Display symbols
./sim6502 --preset c64 --show-symbols program.asm
```

## Known Limitations

- Simple assembler syntax (not full-featured)
- No macro support
- Limited error messages
- Single-file programs only

### Decimal Mode (BCD) Note

ADC and SBC always perform correct BCD arithmetic when the D (Decimal) flag is set, across all supported processor variants (6502, 65C02, 65CE02, 45GS02). The NMOS 6502 hardware has undefined N, V, and Z flag behavior in decimal mode, but this simulator produces well-defined, correct flag values for all variants. Programs relying on the exact undefined 6502 flag state in decimal mode may see different results.

## Future Enhancements

- [ ] Conditional breakpoints
- [ ] Watchpoints (memory monitoring)
- [ ] Step-through mode
- [ ] Profiling information
- [ ] Symbolic debugging (source-level)
- [ ] More predefined symbol tables
- [ ] Custom processor definitions

## TODO

- [ ] Address `unused variable` and `unused parameter` compiler warnings.
- [ ] Address `__builtin_strncpy` output may be truncated` compiler warnings.

## Contributing

Contributions welcome! Areas of interest:
- Additional symbol tables
- Better error messages
- Assembly language extensions
- Performance improvements

## License

Proprietary - See LICENSE file (for now). At some future point the license will go back to open source.

## Credits

Developed as a comprehensive 6502 development tool for learning and hobby projects.

## Support

For issues, questions, or feature requests, please create an issue or consult the documentation.

---

**Version**: 2.0  
**Last Updated**: 2026-02-28
**Status**: Production Ready ✅

Perfect for:
- Learning 6502 assembly
- Debugging programs
- Understanding interrupt handling
- Analyzing Commodore system memory maps
- Modern 8-bit computer development
