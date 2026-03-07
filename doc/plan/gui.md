# GUI Environment Design — sim6502
## Cross-Platform Debugger / IDE Front-End

---

## 1. Goals & Design Philosophy

- **Cross-platform**: Linux, macOS, Windows from a single C/C++ codebase
- **Customizable layout**: Every section of the UI is a "pane" the user can move, resize, detach, stack, or close
- **Non-destructive**: The existing CLI (`sim6502`) continues to work exactly as-is
- **Immediate-mode rendering**: The UI redraws from live simulator state on every frame — no awkward sync logic
- **Minimal dependencies**: Prefer libraries that are vendorable / header-only

---

## 2. Technology Stack

### 2.1 GUI Framework — Dear ImGui (Docking Branch)

Dear ImGui is the recommended toolkit because:

- Written in C++ but trivially callable from C via thin wrappers
- The **ImGui Docking** branch provides a full drag/drop docking system out of the box — this is exactly the "user-rearrangeable panes" feature required
- Immediate-mode rendering naturally fits a simulator that advances state on a step/run cycle
- No toolkit-level event loop to fight: the app controls timing
- Vendorable as source files (no external package manager required)
- Active development, large community, well-documented

**Backend**: SDL2 + OpenGL3 (the reference backend shipped with ImGui)

- SDL2: cross-platform window, input, and context creation
- OpenGL3: available on all target platforms; Metal/D3D11 backends can be swapped later
- Total extra runtime deps: `libSDL2`, `libGL` — both trivially available on all platforms

### 2.2 Build System Integration

Add an optional GUI target to the existing Makefile:

```makefile
gui: $(OBJS_CORE) gui/main.cpp gui/imgui/...
    $(CXX) $(CFLAGS_GUI) -o sim6502-gui ...
```

The CLI binary (`sim6502`) is untouched. `sim6502-gui` is a separate target.

---

## 3. Core Architecture

### 3.1 Simulator Core as a Library

The first structural change: extract the simulator's engine from `main()` into a **library API** (`libsim6502`).

This means refactoring `sim6502.c` so that:

- A struct `sim_session_t` holds all mutable state: `cpu_t`, `memory_t`, dispatch table, symbol table, breakpoints, trace state, loaded program path
- A header `sim_api.h` exposes the public interface:

```c
// Lifecycle
sim_session_t *sim_create(const char *processor);
void           sim_destroy(sim_session_t *s);

// Program loading
int  sim_load_asm(sim_session_t *s, const char *path);
int  sim_load_bin(sim_session_t *s, const char *path, uint16_t load_addr);

// Execution control
int  sim_step(sim_session_t *s, int count);     // step N instructions
int  sim_run(sim_session_t *s);                 // run until break/BRK/STP
void sim_reset(sim_session_t *s);

// Assembler / disassembler
int  sim_assemble_line(sim_session_t *s, const char *text, uint16_t addr);
int  sim_disassemble(sim_session_t *s, uint16_t addr, int count,
                     char *out_buf, size_t out_len);

// State inspection
cpu_t    *sim_get_cpu(sim_session_t *s);        // direct pointer — read only
uint8_t   sim_mem_read(sim_session_t *s, uint16_t addr);
void      sim_mem_write(sim_session_t *s, uint16_t addr, uint8_t val);

// Breakpoints
int  sim_break_set(sim_session_t *s, uint16_t addr, const char *cond);
void sim_break_clear(sim_session_t *s, uint16_t addr);

// Symbol table
const char *sim_sym_by_addr(sim_session_t *s, uint16_t addr);
uint16_t    sim_sym_by_name(sim_session_t *s, const char *name);

// Callbacks (optional, for async notification)
typedef void (*sim_event_cb)(sim_session_t *, int event, void *userdata);
void sim_set_event_callback(sim_session_t *s, sim_event_cb cb, void *ud);

// Execution history (time travel)
void sim_history_set_depth(sim_session_t *s, uint32_t max_entries);
int  sim_history_step_back(sim_session_t *s, int count);
int  sim_history_step_fwd(sim_session_t *s, int count);
int  sim_history_jump_to_cycle(sim_session_t *s, uint64_t cycle);
int  sim_history_depth(sim_session_t *s);
int  sim_history_position(sim_session_t *s);
int  sim_history_rewind_to(sim_session_t *s, const char *condition);
```

Events of interest: `SIM_EVENT_BREAK`, `SIM_EVENT_BRK`, `SIM_EVENT_STP`, `SIM_EVENT_STEP`.

The `main()` in `sim6502.c` becomes a thin wrapper that creates a session and calls the API — CLI behaviour is preserved exactly.

The GUI binary links the same object files and calls the same API.

### 3.2 Threading Model

```
Main thread (GUI):          Simulation thread:
  ImGui frame loop    <-->    sim_step() / sim_run()
  reads cpu/memory            blocks on a mutex when paused
  sends commands              runs freely when in "run" mode
```

- A single `sim_mutex_t` guards the `sim_session_t`
- GUI locks for reads (register snapshot, memory reads for display) and writes (poke, set register)
- When the user hits "Run", simulation thread is released; "Pause" re-acquires
- A "steps per frame" knob (default: unlimited in run mode, 1 in step mode) controls speed

### 3.3 Execution State Machine

```
IDLE  -->  (load program)  -->  READY
READY -->  (step/run)      -->  RUNNING
RUNNING --> (breakpoint)   -->  PAUSED
RUNNING --> (BRK/STP)      -->  FINISHED
PAUSED  --> (step)         -->  PAUSED  (single step)
PAUSED  --> (run)          -->  RUNNING
any     --> (reset)        -->  READY
```

The GUI reflects state via toolbar button availability and status bar text.

---

## 4. Pane System

Every visible panel is a **pane** — an independent ImGui window participating in the docking space.

### 4.1 Pane Lifecycle

Each pane type is registered at startup:

```c
typedef struct {
    const char *id;           // unique window ID (used by ImGui and INI save)
    const char *title;        // display name in title bar
    void (*draw)(pane_ctx_t *); // render function, called every frame if visible
    bool  open;               // current visibility
} pane_def_t;
```

Users can:
- Close a pane (X button on title bar)
- Re-open it from a **View** menu
- Drag a pane to a new docking position (dock to edges, tabs, float)
- Split a docking area horizontally or vertically by dropping
- Detach a pane to a separate OS window

Layout (open panes + docking positions) is saved to an INI file (`sim6502-gui.ini`) via ImGui's built-in INI persistence, extended with any pane-specific settings.

### 4.2 Multi-Instance Panes

Some pane types support multiple simultaneous instances (e.g., two memory views at different addresses). The user can open additional instances from the View menu ("Add Memory View"). Each instance has a unique ImGui ID suffix (`##mem_1`, `##mem_2`, etc.) and independently persisted settings.

---

## 5. Pane Catalogue

### 5.1 Register Display

**Purpose**: Live view of all CPU registers and flags.

**Modes selectable within the pane**:
- *Compact*: Single-line hex dump (`A=42 X=10 Y=20 S=FF PC=0200`)
- *Expanded*: Each register on its own row with labels, decimal and hex
- *Diff*: Registers that changed since last step are highlighted in a different colour

**Contents**:
- A, X, Y registers (always)
- Z, B registers (shown/hidden based on active processor)
- S (stack pointer — 8-bit or 16-bit)
- PC (program counter)
- P register shown as individual flag bits: N V – B D I Z C with colour coding (set=green, clear=dim)
- Cycle counter (decimal)
- EOM prefix active indicator (45GS02)

**Interactive**: Click on a register value to edit it inline (calls `sim_set_reg()`).

---

### 5.2 Memory View

**Purpose**: Hex + ASCII dump of any memory region.

**Modes**:
- *Hex Dump*: Traditional 16 bytes/row with address, hex bytes, ASCII sidebar
- *Decimal View*: Bytes displayed as unsigned decimal
- *Byte Grid*: Compact grid for scanning large ranges
- *Stack View* (specialisation): Always shows the stack region ($0100–$01FF), current S pointer highlighted

**Features**:
- Address navigation bar (type hex address, press Enter)
- "Follow PC" toggle: region scrolls to keep PC visible
- "Follow SP" toggle: region scrolls to keep S visible
- Bytes written by the most recent instruction are highlighted for one frame
- Right-click on a byte: set breakpoint, add symbol, poke value
- Search bar: find byte sequence or ASCII string
- Colour-coded page boundaries

**Configurable**: bytes per row, font size, highlight colour, address width (16 vs 28-bit for far memory).

---

### 5.3 Disassembly View

**Purpose**: Live disassembly of the region around PC (or a pinned address).

**Modes**:
- *Follow PC*: Always centres the listing on the current program counter; current instruction row highlighted
- *Pinned*: Displays a fixed address range; does not scroll automatically
- *Source Mix*: If the source `.asm` file is available, interleaves source lines between disassembly rows (requires line-number tracking in the assembler pass)

**Columns** (toggleable):
- Address (hex)
- Raw bytes (hex)
- Mnemonic + operand
- Addressing mode name
- Cycle count (from opcode table, for active processor)
- Symbol name (if address matches a known symbol)
- Comment (from source file, if loaded)

**Interactive**:
- Click on an address to set/clear a breakpoint (shown as a red dot in the gutter)
- Double-click on an operand address to navigate memory view to that address
- Right-click: "Run to here", "Set PC here", "Jump here"

---

### 5.4 CLI / Console

**Purpose**: Text-based REPL mirroring the existing `-I` interactive mode.

**Modes**:
- *Command*: Accepts all existing interactive commands (`step`, `mem`, `regs`, `break`, `asm`, `disasm`, etc.) via a text input at the bottom; history recalled with Up/Down
- *Log*: Read-only view of trace output or execution log
- *Combined*: Command input + scrolling output above it (default)

**Features**:
- Command history (persisted to disk)
- Auto-completion for command names, register names, symbol names
- Output colourised: addresses in one colour, register values in another, errors in red
- "Clear" button
- Pinnable scrollback limit (e.g., 10000 lines)

This pane is the bridge for power users who prefer typing over clicking.

---

### 5.5 Instruction Reference

**Purpose**: Browse and search the full opcode table for the active processor.

**Modes**:
- *By Mnemonic*: Alphabetical list of all mnemonics; expand to see each addressing mode variant
- *By Opcode*: $00–$FF grid; click a cell for details
- *Search*: Type a mnemonic or hex byte; matching rows highlighted

**Detail view** (shown in a side panel when a row is selected):
- Mnemonic and description
- All addressing modes with syntax, opcode byte(s), byte count, cycle counts per processor variant
- Flags affected (N V B D I Z C) — modified / cleared / set / unchanged
- Notes (undocumented behaviour, page-crossing penalty, etc.)
- For 45GS02: quad-word prefix details

**Processor filter**: Switching the active processor (dropdown at top of pane) re-populates the table.

---

### 5.6 Breakpoint Manager

**Purpose**: Central view of all breakpoints.

**Columns**:
- Enable/disable toggle (checkbox)
- Address (hex) — editable
- Condition expression — editable inline
- Hit count
- Action: "Pause" (default), "Log and continue", "Run command"

**Interactive**:
- "+" button adds a new breakpoint row
- Trash icon removes a row
- Double-click address to navigate disassembly to that address
- Import/export breakpoints to a text file

---

### 5.7 Symbol Table Browser

**Purpose**: Browse, search, and edit the loaded symbol table.

**Columns**: Address, Name, Type (Label / Variable / I/O Port / Trap / etc.), Comment.

**Features**:
- Search / filter by name prefix
- Click on address → navigate disassembly/memory to that address
- Add symbol inline
- Load additional symbol files (drag & drop `.sym` file, or file picker)
- Preset selector: C64 / C128 / MEGA65 / X16 (calls `--preset` logic)

---

### 5.8 Execution Trace Log

**Purpose**: Live log of every executed instruction.

**Columns** (toggleable): Cycle#, PC, Bytes, Mnemonic, Operand, Registers snapshot.

**Modes**:
- *Tail*: Auto-scroll to latest (default during run)
- *Pinned*: Freeze at current scroll position for examination
- *Filtered*: Show only rows matching a mnemonic or address range

**Features**:
- Ring buffer with configurable depth (e.g., last 100k instructions)
- Export to text/CSV
- Click a row to navigate disassembly/memory to that PC

---

### 5.9 Source Editor / Viewer

**Purpose**: Display (and optionally edit) the loaded `.asm` source file.

**Modes**:
- *View*: Read-only, current PC line highlighted, line numbers shown
- *Edit*: Basic text editing; "Assemble & Reload" button re-runs the assembler pass
- *Diff*: Show what memory bytes were written at which source line (requires assembler line tracking)

**Features**:
- Syntax highlighting (mnemonics, labels, comments, numbers)
- Gutter breakpoint indicators (sync with Breakpoint Manager)
- Search and jump-to-line

---

### 5.10 Stack Inspector

**Purpose**: Human-readable view of the 6502 stack.

**Content**:
- Current SP highlighted, stack frames annotated if return addresses are recognisable symbols
- Bytes shown as hex + potential interpretation (addresses shown as symbol names if matched)
- "Depth" counter

---

### 5.11 I/O Monitor

**Purpose**: Watch a configurable set of memory-mapped addresses in real time.

**Per-row**:
- Address (hex), symbol name (auto-filled from symbol table), current byte value (hex + decimal + binary), last-written cycle#, change-count

**Features**:
- Add/remove rows; save watch list to file
- Alarm highlight when a watched address changes
- Works well for monitoring C64 VIC registers, SID, CIA, etc.

---

### 5.12 Processor Status

**Purpose**: Detailed view of the active processor's capabilities and current configuration.

**Content**:
- Active processor name and description
- All flag bits with names and current state (colour-coded)
- EOM/MAP register state (45GS02)
- Clock speed / cycles-per-second display (computed from wall clock vs cycle counter)
- Interrupt vectors ($FFFA/$FFFC/$FFFE) and current state of IRQ/NMI lines

---

### 5.13 Statistics / Profiler

**Purpose**: Instruction-level profiling and memory access statistics.

**Modes**:
- *Instruction Histogram*: Bar chart of most-executed mnemonics during last run
- *Address Heatmap*: 256×256 grid (one pixel per byte); colour intensity = access count
- *Cycle Budget*: Breakdown of cycles spent in each instruction category

**Controls**: "Reset counters" button; live update vs snapshot mode.

---

### 5.14 VIC Viewer

**Purpose**: Render a live graphical view of video memory and sprite data using the current VIC-II, VIC-III, or VIC-IV register state, matching how a real Commodore/MEGA65 machine would display it.

**Chip variant selection** (auto-detected from active processor / symbol preset, or overridden manually):
- *VIC-II*: C64/C128 video chip; registers $D000–$D3FF; 320×200 pixel canvas
- *VIC-III*: C65 video chip (superset of VIC-II); registers $D000–$D7FF; adds 80-column text, interlace, bitplane modes
- *VIC-IV*: MEGA65 video chip (superset of VIC-III); registers $D000–$DFFF; adds full-colour mode, 16-bit char pointers, hardware sprites up to full-screen

**Display modes rendered** (determined by reading the relevant mode/control registers):

VIC-II:
- Standard Character Mode (40×25, 8×8 cells from character ROM base)
- Multicolour Character Mode
- Standard Bitmap Mode (320×200, 1bpp per 8×8 cell)
- Multicolour Bitmap Mode (160×200, 2bpp)
- Extended Colour Mode (background registers per character)

VIC-III additions:
- 80-column text mode
- Bitplane graphics modes (up to 8 bitplanes, 640×400)
- Interlaced / double-resolution modes

VIC-IV additions:
- Full-colour (FC) character mode (each character cell uses its own 8-byte palette entry)
- 16-bit char pointers (characters drawn from any 28-bit address)
- Sprites: up to 8 hardware sprites, each up to full-screen width; position, enable, color, and pointer registers parsed automatically

**Sub-panes** (tab-selectable within the Viewer):

- *Screen*: Live rendered frame at native resolution, scaled to fit pane, updated on each step/run pause or on demand. Optional scanline / pixel-exact zoom overlay.
- *Sprites*: Grid showing all 8 (VIC-II/III) or extended (VIC-IV) sprites. Each cell: rendered sprite bitmap, XY position, colour, multicolour flag, enable state. Click a sprite to jump memory view to its data pointer address.
- *Char Set*: All 256 characters from the current character base address rendered as a 16×16 tile grid. Hover to see char index, click to jump memory view to that character's 8 bytes.
- *Color RAM*: 40×25 (or 80×25) colour attribute grid; each cell shows its 4-bit (VIC-II) or 8-bit (VIC-IV) colour nybble as a coloured square.
- *Registers*: Parsed decode of all VIC register values (addresses, names, current values, meanings) — a read-only structured view, not raw hex.

**Features**:
- "Freeze frame" checkbox: locks the rendered image while execution continues, for comparison
- Export frame as PNG (`stb_image_write.h` — single-header, vendorable, no external deps)
- Export individual sprite as PNG
- Export character set tile sheet as PNG
- Right-click on rendered pixel → "Go to memory address that wrote this pixel" (requires pixel→address mapping, computed from mode + base register values)
- VIC register change highlighting: registers altered since last step shown in amber in the Registers sub-pane

**Implementation note**: The viewer reads raw memory and VIC register bytes; it does not require a full hardware simulation of the VIC timing pipeline. Rendering is done in software into a CPU-side pixel buffer uploaded to an OpenGL texture each frame.

---

### 5.15 Execution History / Time Machine

**Purpose**: Record a complete forward/backward execution history so the user can rewind, inspect, and replay any point in the simulation's past without restarting.

**Core data structure** (`history_entry_t`):
```c
typedef struct {
    cpu_t    cpu;               // full register snapshot (~32 bytes)
    uint32_t cycle;             // cycle counter at this point
    uint16_t mem_delta_count;   // number of memory bytes changed by this instruction
    struct { uint16_t addr; uint8_t old_val; uint8_t new_val; }
             mem_delta[16];     // sparse delta: only bytes written by this instruction
                                // 16 slots covers any realistic single instruction
} history_entry_t;
```

A ring buffer of configurable depth (default: 1,000,000 entries ≈ ~100 MB at ~100 bytes/entry) is maintained in the simulation thread. The buffer uses power-of-two sizing for O(1) index wrapping.

After every `sim_step()`, the engine:
1. Records the pre-step cpu_t and memory write-log contents into a new history entry
2. Advances the ring-buffer write pointer

**API additions to `sim_api.h`**:
```c
// History control
void sim_history_set_depth(sim_session_t *s, uint32_t max_entries);
int  sim_history_step_back(sim_session_t *s, int count);   // restore N entries
int  sim_history_step_fwd(sim_session_t *s, int count);    // re-apply N entries (if any)
int  sim_history_jump_to_cycle(sim_session_t *s, uint64_t cycle);
int  sim_history_depth(sim_session_t *s);    // entries currently stored
int  sim_history_position(sim_session_t *s); // 0 = present, N = N steps back

// Reverse breakpoint: run backwards until condition is true
int  sim_history_rewind_to(sim_session_t *s, const char *condition);
```

Stepping back applies the inverse memory delta (restoring `old_val` bytes) and replaces `cpu_t` with the snapshot, giving a faithful undo of both register and memory state.

**History pane contents**:

- *Timeline slider*: horizontal bar spanning the full ring buffer; dragging scrubs to that point in history; current position shown as a vertical cursor; "present" is the rightmost end
- *Step Back* button (or Shift+F7) / *Step Forward* button (or Shift+F8)
- *Rewind to Breakpoint*: runs backwards through history entries until the active breakpoint condition becomes true; useful for finding "how did A get this value?"
- *History Table*: scrollable list of past instructions (ring buffer entries), columns: Cycle#, PC, Mnemonic, A, X, Y, S, P, Memory writes (abbreviated). Click any row to jump to that state.
- *Depth / Memory usage* display: "Storing N entries / M MB"
- *History depth* setting (spinner, in the pane settings)

**Modes**:
- *Enabled*: history recording active (default; small per-step overhead)
- *Disabled*: recording paused; existing buffer retained but no new entries written (useful during high-speed bulk runs)
- *Cleared*: buffer wiped; recording resumes from current state

**CLI additions**: `stepback [n]` (alias `sb`) and `stepfwd [n]` (alias `sf`) commands in the interactive REPL.

---

### 5.16 Remote Hardware Integration

**Purpose**: Connect the GUI to a real or emulated machine running actual code, using the same display panes already built for the internal simulator. When connected, all panes (registers, memory, disassembly, VIC viewer, etc.) are driven from the remote target rather than the internal simulation.

**Connection bar** (toolbar element, always visible):

```
[Target: ▼ Internal Sim] [Host/Port: ________] [Connect] [Disconnect] [Status: ●]
```

Target options:
1. *Internal Sim* (default) — the built-in `sim_session_t` as described in §3
2. *VICE Remote Monitor* — TCP connection to a running VICE instance
3. *M65dbg Serial* — USB serial connection to a physical MEGA65
4. *M65dbg Ethernet* — TCP connection to a MEGA65 over LAN

The internal simulator remains fully functional when disconnected.

---

#### 5.16.1 VICE Remote Monitor Protocol

VICE exposes a text-based remote monitor on a TCP port (default 6510, configurable in VICE preferences under `Settings → Remote Monitor`).

**Connection setup**:
- Host and port fields (default `localhost:6510`)
- VICE must be launched with the remote monitor enabled: `x64 -remotemonitor -remotemonitoraddress 127.0.0.1:6510`

**Protocol mapping** — VICE monitor command → GUI action:

| GUI action | VICE command sent |
|---|---|
| Read registers | `r` |
| Read memory `$1000`–`$100F` | `m 1000 100f` |
| Write byte `$1000 = $42` | `> 1000 42` |
| Step 1 instruction | `z` |
| Run (continue) | `g` |
| Halt / pause | (disconnect then reconnect; or `CTRL+C` signal) |
| Set breakpoint `$1234` | `break 1234` |
| Clear breakpoint | `del <n>` |
| Reset | `reset` |
| Disassemble | `d 1000 100f` |

**Limitations**: VICE's monitor protocol is line-oriented and not designed for high-frequency polling. Register/memory reads are rate-limited to avoid stalling VICE. GUI updates occur at ~10 Hz when connected rather than per-step.

---

#### 5.16.2 M65dbg Serial Protocol

The MEGA65 hardware exposes a debug interface via its USB serial port (implemented in the MEGA65 FPGA core, not by the CPU). The m65dbg tool by Paul Gardner-Stephen documents and implements the protocol.

**Connection setup**:
- Serial device field (e.g. `/dev/ttyUSB1` on Linux, `COM3` on Windows)
- Baud rate: 2,000,000 bps (2 Mbps — the standard MEGA65 debug serial rate)
- Connect / Disconnect button

**Protocol operations** (binary command/response over serial):

| Operation | Description |
|---|---|
| Read memory | Read arbitrary byte ranges from the 28-bit address space ($000000–$FFFFFFF) |
| Write memory | Write bytes to 28-bit address space |
| Read CPU state | Request current register values (A, X, Y, Z, B, SP, PC, P, cycle count) |
| Set breakpoint | Write a breakpoint address to the MEGA65 debug registers |
| Step | Execute one instruction on the real hardware |
| Continue / halt | Resume or freeze the CPU |
| Reset | Trigger a hardware reset |

**28-bit address space**: M65dbg gives access to the full MEGA65 flat address space. The Memory View pane's 28-bit address mode (already planned) maps directly onto this capability when connected via M65dbg.

**MEGA65-specific extras**: when connected via M65dbg, the VIC-IV Viewer pane reads VIC-IV registers live from hardware, giving a true reflection of what the screen is actually displaying.

---

#### 5.16.3 M65dbg Ethernet Protocol

The MEGA65 network stack supports a debug listener on TCP port 4510, exposing the same command set as the serial protocol but over Ethernet.

**Connection setup**:
- IP address field (e.g. `192.168.1.x`) and port (default `4510`)
- Connect / Disconnect button

**Advantages over serial**:
- No USB cable required
- Higher effective bandwidth for bulk memory reads (full frame buffer dumps for VIC-IV viewer)
- Multiple simultaneous clients possible (monitor + AI agent, for example)

The same protocol adapter used for serial can be used for Ethernet with only the transport layer changed (raw serial vs TCP socket).

---

#### 5.16.4 Protocol Abstraction Layer

To avoid duplicating GUI logic, a thin `remote_target_t` interface abstracts all three external protocols:

```c
typedef struct {
    int  (*read_regs)(void *ctx, cpu_t *out);
    int  (*read_mem)(void *ctx, uint32_t addr, uint8_t *buf, uint16_t len);
    int  (*write_mem)(void *ctx, uint32_t addr, const uint8_t *buf, uint16_t len);
    int  (*step)(void *ctx, int count);
    int  (*run)(void *ctx);
    int  (*halt)(void *ctx);
    int  (*reset)(void *ctx);
    int  (*set_break)(void *ctx, uint32_t addr, const char *cond);
    int  (*clear_break)(void *ctx, uint32_t addr);
    void (*disconnect)(void *ctx);
} remote_target_ops_t;
```

The `sim_session_t` (internal) also exposes this interface, so all display panes operate identically regardless of whether they are reading from the internal simulator or a real MEGA65.

---

## 6. Layout & UX

### 6.1 Default Layout

When launched with no saved INI, the default layout is:

```
+------------------------------------------+
|  Toolbar: [Load] [Step] [Run] [Pause]    |
|           [Reset] [Processor▼] [Speed▸]  |
+----------+--------------------+----------+
| Register |                    | Breakpt  |
| Display  |  Disassembly View  | Manager  |
|          |                    |          |
+----------+                    +----------+
| Stack    |                    | Symbol   |
| Inspector|                    | Browser  |
+----------+--------------------+----------+
|      Memory View              |   CLI    |
|                               | Console  |
+-------------------------------|----------+
|  Status bar: state / PC / cycles        |
+-----------------------------------------+
```

All panes are docked initially; the user can undock any by dragging.

### 6.2 Toolbar

Persistent top bar (not a pane):
- **Load** (file picker for `.asm` or binary)
- **Step** (1 instruction; hold Shift for N)
- **Run** / **Pause** (toggle)
- **Reset**
- **Processor** dropdown (live switch; rebuilds dispatch table)
- **Speed** slider: "Slow" (1 step/frame) → "Fast" (unlimited, turbo)
- **View** menu: toggle all pane types, open additional instances
- **Layout** menu: save layout preset, load layout preset, reset to default

### 6.3 Status Bar

Persistent bottom bar:
- Left: Execution state (`IDLE` / `RUNNING` / `PAUSED at $XXXX` / `FINISHED`)
- Centre: `PC=XXXX  A=XX X=XX Y=XX S=XX  Cycles=NNNNNN`
- Right: Simulator speed (instructions/sec), loaded file name

### 6.4 Theming

- Default: dark theme (matches ImGui's dark palette)
- Light theme option
- Colour tokens for: changed-register, breakpoint active, current PC, written bytes, flag set/clear — all user-editable via a Preferences pane

---

## 7. Persistence

### 7.1 INI File (`sim6502-gui.ini`)

ImGui writes this automatically. Extended with a custom section `[sim6502]` for:
- Last loaded file path
- Active processor
- Per-pane settings (address for pinned memory views, trace depth, etc.)
- Layout preset name (if a named preset was loaded)

### 7.2 Layout Presets

Users can save the current docking layout + open panes as a named preset (e.g., "debug session", "instruction reference only"). Presets stored as separate JSON files in a config directory.

---

## 8. Keyboard Shortcuts

| Key | Action |
|-----|--------|
| F5 | Run |
| F6 | Pause |
| F7 | Step into (1 instruction) |
| F8 | Step over (step past JSR) |
| F9 | Set/clear breakpoint at current PC |
| Shift+F7 | Step back 1 instruction (history) |
| Shift+F8 | Step forward 1 instruction (history re-apply) |
| Ctrl+Shift+R | Rewind to last breakpoint (reverse-continue) |
| Ctrl+L | Load file |
| Ctrl+R | Reset |
| Ctrl+G | Go to address (prompt) |
| Ctrl+F | Find in active pane |
| Ctrl+Shift+F | Find in disassembly |
| Ctrl+Shift+E | Export current VIC frame as PNG |
| ` (backtick) | Focus CLI console |
| Esc | Pause if running |

---

## 9. Directory Structure

```
6502-simulator/
├── src/                   (unchanged — core simulator)
├── gui/
│   ├── main.cpp           Entry point for GUI binary
│   ├── app.h / app.cpp    App state, threading, event loop
│   ├── sim_api.h          Public C API to simulator core (see §3.1)
│   ├── panes/
│   │   ├── pane.h              pane_def_t interface
│   │   ├── registers.cpp
│   │   ├── memory_view.cpp
│   │   ├── disassembly.cpp
│   │   ├── cli_console.cpp
│   │   ├── instr_ref.cpp
│   │   ├── breakpoints.cpp
│   │   ├── symbols.cpp
│   │   ├── trace_log.cpp
│   │   ├── source_editor.cpp
│   │   ├── stack_inspector.cpp
│   │   ├── io_monitor.cpp
│   │   ├── proc_status.cpp
│   │   ├── statistics.cpp
│   │   ├── vic_viewer.cpp      VIC-II/III/IV graphical renderer + sprite/char/color panes
│   │   ├── history.cpp         Execution History / Time Machine pane
│   │   └── remote_connect.cpp  Connection bar + protocol UI
│   ├── remote/
│   │   ├── remote_target.h     remote_target_ops_t abstraction interface
│   │   ├── vice_monitor.cpp    VICE remote monitor TCP protocol adapter
│   │   ├── m65dbg_serial.cpp   M65dbg USB serial protocol adapter
│   │   └── m65dbg_eth.cpp      M65dbg Ethernet (TCP port 4510) protocol adapter
│   ├── vic/
│   │   ├── vic2.h / vic2.cpp   VIC-II register decode + software renderer
│   │   ├── vic3.h / vic3.cpp   VIC-III extensions
│   │   └── vic4.h / vic4.cpp   VIC-IV extensions (MEGA65)
│   ├── history.h               history_entry_t ring buffer (used by sim_api)
│   ├── stb_image_write.h       Single-header PNG export (vendored, MIT)
│   ├── imgui/                  Dear ImGui source (vendored, docking branch)
│   └── backends/               SDL2+OpenGL3 backend files from ImGui
├── Makefile               Add 'gui' target
├── sim6502-gui.ini        (generated at runtime)
└── ...
```

---

## 10. Phased Implementation

### Phase 1 — Core Infrastructure ✓
1. [x] Refactor `sim6502.c` to expose `sim_api.h` library interface; verify CLI still passes all tests
2. [x] Set up `gui/` directory, ImGui+SDL2+OpenGL skeleton, main loop, docking space
3. [x] Toolbar (load, step, run, pause, reset, processor selector)
4. [x] Status bar

### Phase 2 — Essential Panes ✓
5. [x] Register Display (compact + expanded modes)
6. [x] Disassembly View (follow-PC mode)
7. [x] Memory View (hex dump, follow-PC/SP)
8. [x] CLI Console (all existing interactive commands)

### Phase 3 — Debugging Panes ✓
9. [x] Breakpoint Manager
10. [x] Execution Trace Log
11. [x] Stack Inspector
12. [x] I/O Monitor / Watch

### Phase 4 — Reference & Analysis ✓
13. [x] Instruction Reference (mnemonic browser + detail panel)
14. [x] Symbol Table Browser
15. [x] Source Editor / Viewer (view mode first, edit later)
16. [x] Statistics / Profiler

### Phase 5 — Polish
17. [X] Theming and colour preferences
18. [x] Keyboard shortcuts
19. [x] Layout presets (save/load)
20. [x] Multi-instance panes (additional memory views)
21. [x] Drag-and-drop file loading
22. [x] <SKIPPED>

### Phase 6 — Graphics & Time Travel (in progress)
23. [x] Execution History ring buffer infrastructure (`sim_history_*` API in `sim_api.c/.h`)
24. [~] History pane: step-back/step-forward buttons in execution bar, history depth indicator ✓; timeline slider, history table, history enable/disable/clear not yet implemented
25. [x] CLI `stepback` / `stepfwd` commands
26. VIC-II renderer: character mode, bitmap mode, sprites (software rasteriser into pixel buffer → OpenGL texture)
27. VIC Viewer pane: Screen, Sprites, Char Set, Color RAM, Registers sub-panes
27a. [x] VIC Viewer pane: Screen (384×272 rendered frame via GL texture; std/MCM/ECM/bitmap modes; scale controls; register summary; View->VIC-II Screen)
27b. [x] VIC Viewer pane: Sprites
27b2. For VIC Sprites, add editor capability. 
27c. VIC Viewer pane: Char Set
27c2. For VIC Char Sets add editor capability. 
27d. VIC Viewer pane: Color RAM
27e. [x] VIC Viewer pane: Registers sub-panes
28. PNG export for frames, sprites, and char sheets (`stb_image_write.h`)
28a. VIC Viewer pane: Standard Screen bitmap.
28b. VIC Viewer pane: Standard text screen w/ character set rendered.
29. VIC-III mode extensions
30. VIC-IV mode extensions (MEGA65 full-colour mode, extended sprites)

### Phase 7 — Remote Hardware
31. `remote_target_ops_t` abstraction layer + connection bar UI
32. VICE remote monitor TCP adapter (`vice_monitor.cpp`)
33. M65dbg serial adapter (`m65dbg_serial.cpp`): device picker, 2 Mbps serial, command set
34. M65dbg Ethernet adapter (`m65dbg_eth.cpp`): IP/port config, TCP transport
35. All display panes driven from remote target when connected
36. VIC-IV live hardware frame capture via M65dbg bulk memory read

### Phase future
99. macOS / Windows build testing and fixes

---

## 11. Build Dependencies

| Dependency | How to obtain | Notes |
|-----------|--------------|-------|
| Dear ImGui (docking) | Vendor in `gui/imgui/` (MIT) | ~30 .cpp/.h files |
| SDL2 | System package or vendor | Window, input, GL context |
| OpenGL | System (always available) | GL 3.3 core profile |
| stb_image_write.h | Vendor in `gui/` (MIT/Public Domain) | Single header; PNG export |
| C++ compiler | g++ / clang++ / MSVC | Only needed for GUI target |
| gcc (existing) | Existing | CLI target unchanged |
| POSIX sockets / Winsock2 | System (always available) | TCP for VICE + M65dbg Ethernet |
| Platform serial API | System (always available) | termios on Linux/macOS; Win32 COM on Windows — for M65dbg serial |

No package manager required; everything except system socket/serial APIs can be vendored for reproducible builds. The serial and socket APIs are part of every supported OS's standard library.
