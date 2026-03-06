# 6502 Simulator — Graphical Debugger (GUI)

The `sim6502-gui` is a comprehensive, IDE-style debugger built with **Dear ImGui**. It provides a real-time, multi-pane view of the processor state and allows for interactive development and deep-dive debugging of 6502 (and compatible) assembly code.

## Key Features & Panes

The GUI is designed with a flexible docking system, allowing you to rearrange, stack, or detach any of the following panes:

### 1. Register Display
- **Live State**: Real-time view of all CPU registers (A, X, Y, Z, B, SP, PC) and status flags.
- **Diff Highlighting**: Registers that changed during the last execution step are highlighted.
- **Interactive**: Click on any register value to edit it directly.

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
To build and run the GUI, you need a C++11 compiler and the following development libraries:

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
