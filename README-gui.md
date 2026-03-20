# 6502 Simulator — Graphical Debugger (GUI)

`sim6502-gui` is a comprehensive, IDE-style debugger built with **wxWidgets** and **wxAUI**. It provides a real-time, multi-pane view of the processor state, hardware devices, and visual outputs, and allows for interactive development and deep-dive debugging of 6502 (and compatible) assembly programs.

## Contents

1. [Launching](#launching)
2. [Requirements & Building](#requirements--building)
3. [Menu Reference](#menu-reference)
4. [Panes Reference](#panes-reference)
5. [Keyboard Shortcuts](#keyboard-shortcuts)
6. [Settings & Layout](#settings--layout)

---

## Launching

```bash
./sim6502-gui               # start with an empty workspace
./sim6502-gui prog.asm      # load and auto-assemble a file on startup
./sim6502-gui prog.prg      # load a binary on startup
```

---

## Requirements & Building

### System Requirements

- **Linux**: `libwxgtk3.2-dev` (or equivalent wxWidgets 3.x package with GL, AUI, STC, and PropGrid support), `libgl-dev`, `pkg-config`.
- **macOS**: `brew install wxwidgets` (ensure the formula includes GL and AUI support).

### Building

```bash
make gui
```

---

## Menu Reference

### File

| Item | Shortcut | Description |
|------|----------|-------------|
| New Project | Ctrl+N | Open the New Project Wizard to scaffold a project from a template |
| Load | Ctrl+L | Load a file by path (`.asm`, `.prg`, `.bin`) |
| Browse to Load... | | Open a file-picker dialog |
| Save Binary/PRG... | | Save the current memory image to a `.bin` or `.prg` file |
| Exit | Alt+F4 | Quit the application |

### Simulation

| Item | Shortcut | Description |
|------|----------|-------------|
| Run | F5 | Begin continuous execution |
| Pause | F6 | Pause execution |
| Step Into | F7 | Execute one instruction |
| Step Over | F8 | Execute one instruction; if it is a JSR, run the subroutine as one step |
| Reset | Ctrl+R | Reset CPU registers to their initial state |
| Toggle Breakpoint | F9 | Toggle an execution breakpoint at the current PC |
| Step Back | Shift+F7 | Undo the last instruction using execution history |
| Step Forward | Shift+F8 | Re-execute a previously undone instruction |
| Reverse Continue | Ctrl+Shift+R | Run backwards through execution history until a breakpoint |
| Throttle Speed | | Open the speed-throttle dialog (`1.0` = C64 PAL ~985 kHz, `0` = unlimited) |

### Machine

| Item | Description |
|------|-------------|
| Add Optional Device... | Attach an additional hardware device to the address space |
| Processor Selection | Radio sub-menu: **6502**, **6502-undoc**, **65C02**, **65CE02**, **45GS02** |
| Machine Selection | Radio sub-menu: **raw6502**, **c64**, **c128**, **mega65**, **x16** |

Switching the machine type reconfigures the I/O device map (VIC-II, SID, CIA, MEGA65 registers, etc.) and resets the simulator.

### View

Panes are organised into four sub-menus. Each item is a check-item that shows or hides the named pane.

| Sub-menu | Panes |
|----------|-------|
| **Core** | Registers, Disassembly, Console, Memory View 1–4 |
| **Debug** | Breakpoints, Trace Log, Stack, Watch List, Snapshot Diff |
| **Analysis** | Instruction Ref, Symbols, Source View, Profiler, Test Runner, Idiom Library |
| **Hardware > Video** | VIC-II Screen, VIC-II Character Editor, VIC-II Sprites, VIC-II Registers |
| **Hardware > Audio** | SID Debugger, Audio Mixer |
| **Hardware** | I/O Devices |

Additional view items:
- **Go to Address...** (Ctrl+G) — jump disassembly to a hex address.
- **Settings** — Font Size, Theme, and Layout sub-menus (see [Settings & Layout](#settings--layout)).

### Window

| Item | Shortcut | Description |
|------|----------|-------------|
| Arrange | Shift+F12 | Re-sequence all active panes to be visible (fixes panes lost off-screen) |

---

## Panes Reference

All panes dock, float, stack, and resize freely using wxAUI. Drag a pane's title bar to reposition it; double-click the title bar to float or re-dock it.

---

### Registers

Displays all CPU registers in a four-column list: **Register**, **Value** (hex), **Dec** (decimal), **Prev** (previous value), and **Flags / Bits**.

**Features:**
- **Diff highlighting**: Registers that changed during the last execution step are shown in red.
- **Inline editing**: Single-click or double-click the **Value** (hex) or **Dec** (decimal) column on any register row to open an inline text editor. Press Enter to commit; press Escape or click away to cancel.
- **Pencil cursor**: Hovering over an editable cell (Value, Dec) or the P-register Flags column shows a pencil cursor to signal interactivity.
- **Status flag toggling**: Clicking within the **Flags / Bits** column on the P-register row toggles individual bits (N, V, U, B, D, I, Z, C) based on the click X-position.
- **CPU-adaptive rows**: The Z and B register rows are automatically shown only when the active processor type is **65CE02** or **45GS02**. For 6502 / 65C02 sessions, these rows are hidden.

---

### Disassembly

Displays a scrolling stream of disassembled instructions centred on the current PC.

**Features:**
- **Breakpoint gutter**: Double-click any row to toggle an execution breakpoint (marked with `●`).
- **PC tracking**: The current PC row is highlighted in blue and kept in view on every step.
- **Symbol labels**: Labels from the loaded symbol table appear next to their addresses.
- **Cycle counts**: The base cycle count for each instruction is shown according to the active processor variant.
- **Address bar**: Type a hex address in the toolbar search field and press Enter to jump the disassembly view to that address (disables PC-follow mode).
- **Right-click context menu**: Right-click any instruction row for: **Set PC to `$XXXX`** (redirect execution to that address), **Go to address in Memory View**, **Add Watch at `$XXXX`**, and (when the instruction targets a memory operand) **Add Watch at Operand**.

---

### Memory Views (1–4)

Up to four independent hex + ASCII/PETSCII dump windows, each showing a configurable memory region.

**Toolbar:**
- **Page Up / Page Down**: Scroll by one page of visible rows.
- **Follow** dropdown: Continuously center the view on **None**, **PC**, **SP**, or **Word Ptr** (follows the 16-bit value stored at a user-specified pointer address).
- **Charset** dropdown: Display the text column as **ASCII** or **PETSCII**.
- **Rename**: Set a custom title for this memory view (e.g., "Zero Page", "Stack").

**Features:**
- Open each view independently from **View > Core > Memory Views**.
- Scroll with the mouse wheel or scrollbar.
- **Inline editing**: Double-click a row to open a hex-byte editor for that address range.
- Each view's scroll position, follow mode, charset, and title persist between sessions.

---

### Console

A full-featured interactive terminal mirroring the CLI monitor mode.

**Features:**
- **Complete command set**: Every CLI monitor command is available here (see the [Interactive Monitor](README.md#interactive-monitor) section in the main README).
- **Command history**: Navigate previous commands with Up/Down arrow keys.
- **Focus shortcut**: Press the backtick key (`` ` ``) from anywhere in the GUI to focus the console input field.
- **Output log**: All simulator output including trace events, memory dumps, and error messages appears in the scrolling output area.

---

### Source View

Displays the original `.asm` source file alongside the disassembly.

**Features:**
- **Current-line marker**: An arrow highlights the source line corresponding to the current PC; the view auto-scrolls to keep it visible during stepping.
- **Syntax highlighting**: Assembly mnemonics, labels, and comments are colour-coded using wxStyledTextCtrl.
- **Breakpoint margin**: Click in the left margin to set or clear a breakpoint on any source line.
- **Find bar**: Press Ctrl+F to reveal an inline search bar. Type to search; press Enter or **Next** to advance, Shift+Enter or **Prev** to go back, Escape or **X** to dismiss.

---

### Profiler

A 256×256 pixel visual map of the 64 KB address space showing execution frequency and cycle consumption.

Two-tab view showing execution data for the full 64 KB address space.

**Toolbar:**
- **Save**: Export the current heatmap as a PNG image.
- **Clear**: Reset all execution hit counts and cycle totals to zero.

**Heatmap tab:**
- **Heatmap**: 256×256 pixel map — one pixel per address. Intensity indicates execution frequency; colours range from black (never executed) through red to yellow (hotspots).
- **Hover tooltip**: Shows address, hit count, and total cycle count for the location under the cursor.
- **Real-time update**: Refreshes at approximately 60 Hz.

**List tab:**
- Sortable table of every address that has been executed, with **Address**, **Hit Count**, and **Cycle Count** (total cycles consumed at that address) columns.
- Click a column header to sort ascending/descending.

---

### Breakpoints

Manages all active execution breakpoints.

**Toolbar buttons:**
- **Add**: Prompts for an address (hex) and an optional condition expression.
- **Delete**: Removes the selected breakpoint.
- **Clear All**: Removes all breakpoints.

Double-clicking a row in the list activates (toggles) the breakpoint.

**Conditional breakpoints** use the same expression syntax as the CLI:

```
break $C000 (A & $40) != 0 && .Z == 1
```

---

### Trace Log

A rolling ring-buffer of recently executed instructions showing address, disassembly, and register state (A, X, Y, SP, P) at the time of execution.

**Features:**
- **Follow mode**: The list automatically scrolls to the newest entry during execution.
- **Cycle counter**: The cumulative cycle count is displayed in the rightmost column.

---

### Stack Inspector

Two-tab view of the hardware stack (page 1, $0100–$01FF):

- **Stack Memory** tab: Hex dump of the entire stack page with the current SP position indicated.
- **Recent** tab: Heuristic analysis of the current stack contents, annotating byte pairs as potential return addresses.

---

### Watch List

Pin memory addresses to monitor their values in real time.

**Toolbar buttons:**
- **Add Watch** (+): Open a dialog to enter an address (hex), an optional label, and a width (1, 2, or 4 bytes).
- **Delete Watch** (−): Remove the selected entry.
- **Clear All**: Remove all watch entries.

**Adding watches (alternative):** Right-click anywhere in the list to open a context menu with the same Add/Delete/Clear All options.

**Columns:** Label, Address, Width, Hex value, Decimal value (and ASCII character for 1-byte watches).

**Features:**
- **Change highlighting**: Entries that changed on the most recent execution step are highlighted in red.
- **Live update**: Values refresh on every display tick while the simulator is running or paused.
- Watch entries (address, label, width) persist between sessions.

---

### Snapshot Diff

Records a memory baseline and tracks every byte that changes after the snapshot is taken.

**Toolbar buttons:**
- **Snap**: Capture the current memory state as a baseline. Subsequent execution records all writes relative to this baseline.
- **Clear**: Invalidate the current snapshot and empty the diff table.

**Columns**: Address, Before value, After value, Writer PC (address of the instruction that made the last write to each location).

The diff table collapses consecutive modified addresses into ranges for readability.

---

### Instruction Ref

Searchable database of every instruction supported by the active processor variant.

**Layout:**
- **Top list**: All mnemonics for the active processor. Type in the filter field to narrow by name.
- **Detail panel**: Clicking a row shows all addressing modes, byte lengths, and cycle counts for that instruction.

---

### Symbols

Search, inspect, and navigate all labels, constants, and annotations loaded from `.sym`, `.list`, or `.sym_add` files.

**Toolbar:**
- **Load**: Load symbols from a `.sym` file.
- **Save**: Save the current symbol table to a `.sym` file.
- **Add**: Add a new symbol (name and address dialog).
- **Delete**: Remove the selected symbol.
- **Filter**: Type to narrow the list by name or address.

**Features:**
- **Navigate on activation**: Double-click a symbol row to scroll the Disassembly pane to that address.
- **Column sorting**: Click any column header to sort by name, type, or address.
- **Columns**: Name, Type (label / constant / inspect / trap), Address.

---

### Test Runner

Validates subroutines against register input/output test vectors.

**Usage:**
1. Enter the **Routine Address** (hex) of the subroutine under test.
2. Enter a **Scratch Address** (hex) — a temporary memory area the test harness may use to save and restore state.
3. Add test vectors in the table: input register values on the left, expected output values on the right.
4. Click **Run All** to execute each vector and see immediate pass/fail results.

---

### Idiom Library (Patterns)

Browser for documented, parameterised assembly snippets.

**Usage:**
1. Select a category and snippet from the list.
2. The preview area shows the full annotated source with parameter descriptions.
3. Click **Copy to Clipboard** to copy the snippet, then paste it into your source file.

Built-in snippets cover 6502 and 45GS02 targets and include: 8-bit and 16-bit multiplication, memory copy/fill, BCD conversion, hardware math routines, and more.

---

### I/O Devices

Live property-grid view of all mapped I/O device registers for the selected machine type.

Shows decoded register state for devices including the CIA, SID, VIC-II, and MEGA65 extended I/O, updated in real time during execution.

---

### VIC-II Screen

OpenGL-rendered live view of the full 384×272 PAL frame (border + active display + all sprites).

**Features:**
- **All display modes**: Standard Character, Multicolour Character, Extended Colour (ECM), Standard Bitmap, Multicolour Bitmap — all rendered correctly.
- **Sprite rendering**: All 8 hardware sprites with X/Y expansion, multicolour, X-MSB, and correct sprite-background priority.
- **Live updates**: The frame re-renders on every display tick, showing the exact state of VIC-II registers and video memory.

---

### VIC-II Character Editor

Full-featured pixel editor for the character set stored in video memory.

**Layout:**
- **Character Selector** (left): 16×16 grid showing all 256 characters in the current character set, rendered with the current colour settings. Click any character to select it for editing.
- **Big Editor** (centre): Zoomed pixel-level editor for the selected character. Click pixels to toggle them; hold and drag to paint. Supports multicolour (MCM) mode where pixel pairs represent four colours.
- **Virtual Screen** (right): 40×25 scratchpad showing the font in use. Click any cell to paint the currently selected character at that position.
- **Controls panel** (far right): Colour pickers for background and three multicolour indices; MCM mode toggle; address mode (auto from VIC-II registers vs. manual); character set load/save.

**Toolbar:**
| Button | Action |
|--------|--------|
| Lock / Unlock | Prevent accidental edits; the editor auto-locks when simulation is running |
| Shift Up / Down / Left / Right | Shift the pixel data of the selected character by one pixel |
| Flip Horizontal / Vertical | Mirror the character |
| Rotate | Rotate the character pixel data |
| MCM Grid | Overlay a 2×1 grid to visualise multicolour pixel boundaries |
| Copy | Copy selected character data to an internal clipboard |
| Paste | Paste clipboard data to the selected character slot |
| Export as ASM | Generate `.byte` assembly statements for the selected character |
| Export as PNG | Save the full character set grid as a PNG image |
| Clear Scratchpad | Reset the virtual screen to all character 0 |
| Load Character Set | Load raw binary data into the character set from a file |
| Save Character Set | Save the character set data to a binary file |
| Undo (Ctrl+Z) | Undo the last pixel edit |

---

### VIC-II Sprites

Unified editor and viewer for all 8 hardware sprites.

**Layout:**
- **Thumbnail strip** (top): Eight sprite thumbnails, one per sprite. Click a thumbnail to select that sprite for editing.
- **Big Editor** (centre): Zoomed pixel-level editor for the selected sprite (24×21 pixels). Click to toggle pixels; hold and drag to paint. Supports multicolour mode.
- **Controls panel** (right): Per-sprite and shared multicolour colour pickers; MCM mode toggle; data address display.

**Toolbar:**
| Button | Action |
|--------|--------|
| Lock / Unlock | Prevent accidental edits while simulation is running |
| Undo | Undo the last pixel edit |
| Rotate 90° | Rotate sprite pixel data 90 degrees clockwise |
| Rotate 180° | Rotate sprite pixel data 180 degrees |
| Invert | Invert all pixels in the sprite |
| Save Blob | Save sprite data to a binary file |
| Load Blob | Load sprite data from a binary file |
| MCM | Toggle multicolour mode for the selected sprite |

---

### VIC-II Registers

Decoded read-only view of all VIC-II control registers displayed in a property grid.

Shows decoded values for: sprite X/Y positions, display mode bits (D011, D016), raster line (D012, 9-bit), memory layout register (D018), interrupt status (D019) and enable (D01A), sprite flags (multicolour, expansion, enable, priority, collision), bank selection, and all colour registers (D020 border, D021–D024 background 0–3, D025–D026 sprite multicolour, D027–D02E per-sprite colour).

---

### SID Debugger

Live property-grid view of SID register state, grouped by voice and section.

**Sections:**
- **Voice 1–3**: Frequency Lo/Hi, Pulse Width Lo/Hi, Control (waveform/gate/sync/ring), Attack/Decay, Sustain/Release.
- **Filter & Volume**: Filter Cutoff Lo/Hi, Resonance/Filter routing, Mode/Volume.

Values update in real time as the simulator executes. The view is read-only; to poke a register use the Console (`write` command) or the Memory View.

---

### Audio Mixer

Per-SID volume and mute controls. The number of SID chips shown adapts to the selected machine type (1 SID for C64/C128/X16; 4 SIDs for MEGA65).

**Per-SID controls:**
- **Vol slider**: Controls master volume at the SID's `$D418` register (0–15). Writes directly to the simulated register.
- **Percentage label**: Shows the current volume as a percentage (0%–100%) next to the slider.
- **Tick marks**: Small labels (0–15) below the slider for reference.
- **Mute checkbox**: Silences the SID by writing volume 0; restores the previous volume when unchecked.

**MEGA65-only (4 SIDs):**
- **Pan slider**: Stereo panning control per SID chip, ranging from L100 (full left) through Centre (0) to R100 (full right). SIDs 1 and 2 default to right; SIDs 3 and 4 default to left to match the MEGA65 hardware layout.

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **F5** | Run (continuous execution) |
| **F6** | Pause execution |
| **F7** | Step Into (1 instruction) |
| **F8** | Step Over (runs JSR body as one step) |
| **F9** | Toggle breakpoint at current PC |
| **Shift+F7** | Step Back (execution history undo) |
| **Shift+F8** | Step Forward (execution history redo) |
| **Ctrl+Shift+R** | Reverse Continue (run backwards to breakpoint) |
| **Ctrl+R** | Reset CPU |
| **Ctrl+N** | New Project Wizard |
| **Ctrl+L** | Focus filename bar / open Load dialog |
| **Ctrl+G** | Go to Address (opens hex-entry popup, scrolls disassembly) |
| **Shift+F12** | Arrange Windows (re-sequences all panes to be visible) |
| **`** (backtick) | Focus the Console input field |
| **Ctrl+F** | Open/close the Find bar in the Source View |

---

## Settings & Layout

Access via **View > Settings**.

### Font Size

Radio sub-menu with sizes 10 px through 24 px. Applies to all panes that use the monospaced list font.

### Theme

| Option | Description |
|--------|-------------|
| Auto (OS) | Follow the operating system light/dark preference |
| Dark | Force dark theme |
| Light | Force light theme |

### Layout

| Option | Description |
|--------|-------------|
| Save Layout... | Save the current pane arrangement to a named file |
| Reset to Default | Restore the factory default pane layout |

The wxAUI layout (pane positions, sizes, and visibility) is automatically persisted between sessions. Use **Window > Arrange** (Shift+F12) to bring any panes back into view if they have been moved off-screen.
