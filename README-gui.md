# 6502 Simulator — Graphical Debugger (GUI)

The `sim6502-gui` is a comprehensive, IDE-style debugger built with **Dear ImGui**. It provides a real-time, multi-pane view of the processor state and allows for interactive development and deep-dive debugging of 6502 (and compatible) assembly code.

## Key Features & Panes

The GUI is designed with a flexible docking system, allowing you to rearrange, stack, or detach any of the following panes:

### 1. Register Display
- **Live State**: Real-time view of all CPU registers (A, X, Y, Z, B, SP, PC) and status flags.
- **Diff Highlighting**: Registers that changed during the last execution step are highlighted.
- **Interactive**: Toggle to **Expanded Mode** to edit any register value directly via hex input.

### 2. Disassembly View
- **Instruction Stream**: Real-time disassembly of memory around the Program Counter.
- **Breakpoint Gutter**: Click in the left margin to toggle breakpoints (red dots).
- **Symbol Integration**: Displays labels and constants from your symbol table directly in the code.
- **Cycle Counts**: Shows the base cycle count for each instruction according to the active processor.

### 3. Memory View
- **Hex + ASCII Dump**: View any region of the 64KB (or 28-bit) memory space.
- **Last-Write Highlighting**: Bytes modified by the most recent instruction are highlighted.
- **Follow Modes**: Toggles to automatically track the Program Counter (PC) or Stack Pointer (SP).
- **Multi-Instance**: Open up to 4 independent memory views at different addresses.

### 4. CLI Console
- **Full REPL**: A full-featured interactive terminal mirroring the CLI monitor mode.
- **Command History**: Navigate previous commands with Up/Down arrows.
- **Binary I/O**: Supports `bload` and `bsave` for loading/saving `.bin` and `.prg` files.

### 5. Source Viewer
- **Original Source**: View your `.asm` file alongside the disassembly.
- **Line Highlighting**: The current execution line is highlighted in the source file.

### 6. Profiler / Heatmap
- **Visual Profiling**: A 256x256 pixel grid representing the 64KB memory map.
- **Hotspots**: Pixel intensity indicates execution frequency or cycle consumption.
- **Optimization Tool**: Quickly identify where your code is spending the most time.

### 7. Stack Inspector
- **Annotated Stack**: A specialized view of Page 1 ($0100-$01FF).
- **Frame Analysis**: Attempts to identify return addresses and stack-pushed parameters.

### 8. Instruction Reference
- **Opcode Database**: Searchable database of every supported instruction.
- **Details**: Shows all addressing modes, byte lengths, and cycle counts for each processor variant.

### 9. Symbol Browser & Watch List
- **Symbol Browser**: Search, inspect, and navigate to any loaded label or variable.
- **Watch List**: Pin specific addresses to monitor their values as the program runs.

### 10. Pattern Library (View menu → Pattern Library)

- **Built-in snippet browser**: All built-in assembly snippet templates organised by category (math, memory, io, time) and processor variant (6502, 65C02, 45GS02/MEGA65).
- **Filter**: Type to narrow the list by name, category, or processor.
- **Detail view**: Full documented source for the selected snippet, including parameter equates and clobber notes.
- **Copy to Clipboard**: One-click copy of the full snippet body, ready to paste into your source file.
- **CLI equivalent**: `list_patterns` / `get_pattern <name>` in the interactive monitor or embedded console.
- **MCP equivalent**: `list_patterns` and `get_pattern` tools.

### 11. Test Runner (View menu → Test Runner)

- **Subroutine test-vector table**: Define input and expected-output register values for any loaded subroutine, then run all vectors with one click.
- **Pass/Fail reporting**: Each row shows per-register pass/fail with expected vs. actual values.
- **JSON export**: Results can be exported as JSON for CI integration.
- **CLI equivalent**: `validate <addr> [REG=val…] : [REG=val…]` in the interactive monitor.
- **MCP equivalent**: `validate_routine` tool.

### 12. Project Wizard (File menu → New Project...)

Quickly bootstrap a new development environment using pre-defined templates.
- **Template Selection**: Choose from minimal, standard C64, or advanced MEGA65 setups.
- **Live Configuration**: Set project name, target directory, and template-specific variables (like `START_ADDR`) directly in the wizard.
- **Auto-Load**: Upon creation, the wizard automatically generates the directory structure, Makefile, and boilerplate code, then loads the main assembly file into the Source Viewer.

### 13. VIC-II Viewer (View menu → VIC-II Screen / Sprites / Registers)

Three panes expose the Commodore 64 VIC-II video chip state for C64 / C128 / MEGA65 development:

#### VIC-II Screen
- **Live rendered frame**: Full 384×272 PAL frame — border, 320×200 active display area, and all enabled hardware sprites — rendered in real time via an OpenGL texture.
- **All display modes**: Standard Char, Multicolour Char, Extended Colour (ECM), Standard Bitmap, and Multicolour Bitmap — rendering automatically follows the current D011/D016 mode register values.
- **Sprites**: All 8 hardware sprites are rendered on top of the background in every mode (bitmap and character), with correct priority ordering (sprite 0 on top), X/Y expansion, multicolour, and X-MSB support.
- **Scale controls**: 1×, 2×, 3× zoom.
- **Freeze**: Lock the last rendered frame while execution continues, for comparison.
- **Mode indicator**: Colour-coded label (green = char mode, yellow = bitmap mode, grey = display off) plus live addresses for Bank, Screen RAM, CharGen / Bitmap Base.
- **CLI equivalent**: `vic2.savescreen [file]` saves the identical 384×272 frame as a PPM. `vic2.savebitmap [file]` saves the 320×200 active area (no border, sprites included) as a PPM.
- **MCP equivalent**: `vic2_savescreen` and `vic2_savebitmap` tools.

#### VIC-II Sprites
- **Unified Sprite Editor**: A comprehensive pane for inspecting and modifying all 8 hardware sprites.
- **Selection Bar**: Click any of the 8 thumbnails at the top to focus the editor on that specific sprite.
- **Interactive Bitmap Editor**: A large pixel grid for direct drawing.
    - **1bpp mode**: Toggle pixels between transparent and the sprite colour.
    - **2bpp (Multicolour) mode**: Cycle through the four available colour indices.
    - **Expansion Support**: The grid dynamically reflects **Expand X** and **Expand Y** settings, doubling pixel size in the editor to match hardware output.
    - **Aspect Ratio**: Corrected pixel proportions to emulate the taller-than-wide PAL display.
- **Hardware Attributes**: Real-time toggles for Enable, Multicolour, Expand X/Y, and Priority ($D01B).
- **Colour Palettes**: Interactive palettes for the sprite's main colour ($D027+N) and shared multicolours ($D025/$D026).
- **Freeze Update**: Locks the visual state of all sprites.
    - **Safe Editing**: Essential for manual drawing; freezing prevents a running program from overwriting your changes in memory while you work.
    - **Capture**: Useful for inspecting high-speed animations or "flicker" effects.
- **CLI equivalent**: `vic2.sprites` prints all 8 sprite states.
- **MCP equivalent**: `vic2_sprites` tool.

#### VIC-II Registers
- **Register table**: D011, D016, D018, D012 (raster line), D019 / D01A (interrupt flags) — each with raw hex and decoded field annotations (ECM/BMM/DEN/RSEL/RST8/yscroll, MCM/CSEL/xscroll, IRQ/RST/MBC/MMC/LP).
- **Video addresses**: VIC bank (with address range), Screen RAM, Char Gen / Bitmap Base (label and value switch based on BMM), Colour RAM.
- **Colour swatches**: 14×14 pixel palette swatches for Border (D020) and BG0–BG3 (D021–D024) with index and colour name.
- **CLI equivalent**: `vic2.regs` in the interactive monitor or the embedded console outputs the same information in text form.
- **MCP equivalent**: `vic2_regs` tool.

## Speed Throttle

The **Run** menu contains a **Throttle Speed** toggle and a scale slider:

- **Throttle Speed** checkbox: when enabled, execution is rate-limited to the configured scale factor.
- **Scale slider** (0.1×–10×): `1.0` = C64 PAL clock (~985 kHz); values above 1 run faster, below 1 run slower than a real C64.
- The throttle state and scale are **persisted** in the ImGui INI file and restored on next launch.
- **CLI equivalent**: `speed [scale]` — query or set the throttle from the interactive monitor or embedded console. `speed` alone prints the current setting.
- **MCP equivalent**: `speed` tool (pass `scale` parameter to set, omit to query).

---

## File Structure

The project is organized into modular C++ components:

- **Core Engine** (`src/core/`):
    - `cpu.h`: Abstract `CPU` base class and `CPUState` for register management.
    - `cpu_6502.cpp`: Implementation of specific CPU variants (6502, 65C02, 45GS02).
    - `memory.h`: Memory bus logic, including MEGA65 DMA and Math Coprocessor.
    - `opcodes/`: Opcode implementations segregated by processor variant.
- **CLI Frontend** (`src/cli/`):
    - `main.cpp`: CLI entry point.
    - `commands/`: Command Pattern implementation for interactive monitor commands.
- **GUI Frontend** (`src/gui/`):
    - `main.cpp`: Main application loop and GUI orchestration.
    - `panes/`: (In Progress) Individual UI component implementations.

---

## Breakpoint Conditions

The GUI and CLI both support complex conditional breakpoints using standard assembler expression syntax.

**Supported Operators**:
- **Arithmetic**: `<<, >>, &, |, ^, ~`
- **Logical**: `&&, ||, !, ==, !=, <, >, <=, >=`
- **Parentheses**: `( )` for grouping and precedence.

**Example Conditions**:
- `(A & $80) != 0` — Stop if bit 7 of A is set.
- `X > $10 && .Z == 1` — Stop if X > 16 and the Zero flag is set.
- `PC >= $C000 && PC <= $CFFF` — Stop if execution is within a specific range.

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **F5** | Run (continuous execution) |
| **F6 / Esc** | Pause execution |
| **F7** | Step Into (1 instruction) |
| **F8** | Step Over (executes JSR body as one step) |
| **F9** | Toggle breakpoint at current PC |
| **Shift+F7** | **Step Back** (undo 1 instruction using history) |
| **Shift+F8** | **Step Forward** (re-execute 1 instruction from history) |
| **Ctrl+R** | Reset CPU (registers clear, PC to start) |
| **Ctrl+L** | Focus the filename bar / Open load dialog |
| **Ctrl+G** | Go to Address (scrolls disassembly and memory) |
| **Ctrl+F** | Focus Search (in Source Viewer or Symbol Browser) |
| **Ctrl+Shift+F** | Focus Disassembly Address Bar |
| **`** (backtick) | Focus the CLI Console Input |

---

## Requirements & Building

### System Requirements
To build and run the GUI, you need a C++17 compiler and the following development libraries:

- **SDL2**: Cross-platform windowing and input handling.
- **OpenGL**: Hardware-accelerated rendering.
- **pkg-config**: For managing library paths.

On **Ubuntu/Debian**:
```bash
sudo apt-get install g++ libsdl2-dev libgl-dev pkg-config
```

On **macOS**:
```bash
brew install sdl2
```

### Building the GUI
Run the following command from the project root:

```bash
make gui
```

This will create the `sim6502-gui` binary. *Note: The build process will automatically fetch the required Dear ImGui source files from GitHub if they are not already present.*

### Launching
```bash
./sim6502-gui
```
You can also pass a filename to open it directly:
```bash
./sim6502-gui examples/hello.asm
```
