# Release and Milestone Schedule

This document outlines the release schedule for the 6502 Simulator, prioritizing stability, usability, and core features for the initial 1.0 release, followed by themed updates for advanced functionality.

---

## Milestone 1.7: Emulator Integration & Configuration
**Goal:** Streamline the simulator setup by importing system ROMs from established emulators and centralizing user preferences.
**Theme:** System Integration & Configuration management.

- [ ] **Time Machine UI:** Add the timeline slider and history table to the Execution History pane, including the "Rewind to Breakpoint" (reverse-continue) feature.
- [ ] **VIC Screen Polish:** Implement scale controls (1x/2x/3x), freeze-frame toggle, and mode/address status indicators.
- [ ] **Unified Settings Dialog:**
  - [ ] Implement a multi-pane `SettingsDialog` (using `wxTreebook` or similar) to replace scattered configuration menus.
  - [ ]**Panes:** 
    - [ ] *General:* Interface scaling, theme selection, and font preferences.
    - [ ] *Emulators:* Paths to VICE and Xemu binaries/data directories.
    - [ ] *ROM Mapping:* Assignment of Kernal, BASIC, and Character ROMs to system targets.
    - [ ] *Speed and Memory:* Allow setting of default speed, machine target and processor and adjustment of built-in caches that are used for things like history etc. 
- [ ] **Emulator Resource Discovery:**
  - [ ] Implement auto-discovery logic to locate VICE and Xemu installations on Linux, macOS, and Windows.
  - [ ] Add a "Scan for ROMs" feature that automatically populates system profiles from discovered emulator data folders.
- [ ] **Binary & Metadata Parsing:**
  - [ ] **VICE Support:** Load and parse `.bin` ROM images and `.sym` symbol files for system-level debugging.
  - [ ] **MEGA65 Support:** Implement a parser for `.M65` bundle files to extract Character and System ROMs.
  - [ ] **PRG/BIN Loading:** Improve the "Load Binary" interface to optionally auto-configure the starting PC based on metadata (e.g., standard C64 `.prg` load addresses).
- [ ] **Project-Specific Configuration:**
  - [ ] Allow `project.toml` to specify a custom ROM set, overriding global settings for specific development tasks.
  - [ ] Extend the `sim_api.h` to support dynamic ROM switching without restarting the simulator session.

---

## Milestone 1.8: Ensure GUI, MCP and CLI are equally functional. 
**Goal:** Get the application into a fully functioning usable state for all 3 states of GUI, MCP and CLI. 
**Theme:** Corrections & Refinement. 

- [X] **Unified Logic:** Consolidate execution logic between GUI and CLI to ensure consistent behavior for all step command variants.
- [ ] **JSON Consistency:** Add `g_json_mode` (`-J`) support to `disasm`, `asm`, `jump`, `write`, `set`, `flag`, `bload`, and `bsave` for robust front-end integration.
- [ ] **Command Migration:** Move remaining CLI handlers from `commands.cpp` to the modular `CommandRegistry` architecture.
- [ ] **Command Standardization:** Rename legacy CLI commands (e.g., `flag` -> `set_flag`, `bload`/`bsave` -> `load`/`save`) to follow standard debugger conventions.
- [ ] **Breakpoint Management:** Implement `list` and `clear` CLI command handlers to expose existing API functionality across all interfaces.
- [ ] **Terminology Alignment:** Rename "patterns" to "Idioms" and "templates" to "Project Templates" across the codebase and documentation.

---

## Milestone 1.9: Architectural Integrity & MCP Robustness
**Goal:** Finalize the library extraction and ensure the MCP server meets modern production standards.
**Theme:** Infrastructure & Decoupling.

- [ ] **CPU-Agnostic Foundation:** Implement the `RegisterInfo` metadata system and virtualize the `step()` execution loop as defined in `.plan/cpu.md`.
- [ ] **Register File Generalization:** Transition the GUI and API from the hardcoded `CPUState` struct to name-indexed register access.
- [ ] **Library Split (Phase 7):** Enforce physical library boundaries via the filesystem and linker for `lib6502-core`, `lib6502-mem`, etc.
- [ ] **MCP Robustness:** Transition to JSON envelopes (`-J`) for robust command completion and improve simulator process lifecycle management.
- [ ] **MCP Modernization:** Upgrade `@modelcontextprotocol/sdk` to `v1.x` and migrate to the latest protocol standards.
- [ ] **Command Completion:** Transition MCP logic from fragile prompt-string matching to robust JSON envelope detection.
- [ ] **Error Propagation:** Improve error surfacing by capturing and returning detailed simulator stderr/stdout to the MCP client.
- [ ] **Keyboard Matrix:** Implement the `key` CLI command and map host keyboard events to the C64 matrix in the GUI.

---

## Milestone 2.0: Visual Debugging & Tooling
**Goal:** Expand debugging capabilities with "time travel" features and advanced visual inspection tools.
**Theme:** Advanced UI & Debugging

- [ ] **Time Machine:** Complete the timeline slider, history table, and implement "Rewind to Breakpoint" (reverse-continue).
- [ ] **VIC Viewer Editors:** Complete the Sprite and Character Set editors and implement the Color RAM sub-pane.
- [ ] **Character ROM Control:** In the VIC-II Character Editor, add an option to copy the ROM character set into RAM (at the currently configured VIC charset base address) and disable the ROM so edits are visible on the VIC screen. Requires toggling the CHAREN bit in CPU port $01 and writing `char_rom` contents to the RAM location the VIC is pointed at.
- [ ] **C++ Header Modernisation:** Convert all `.h` files that currently use C idioms (e.g. `int` for booleans, `typedef struct`, `#define` constants) to proper C++ style — `bool`, `enum class`, `constexpr`, inline member functions — and remove any remaining `stdbool.h` / `stdint.h` guards that are redundant in C++.
- [ ] **Asset Management:** Support PNG export for frames, sprites, and character sheets.
- [ ] **UI Customization:** Support saving/loading custom layout presets and implement user-configurable theming.
- [ ] **Toolchain Extensions:** Implement full ACME assembler support (parsing `label = $addr` and legacy annotations).

---

## Milestone 2.1: Hardware Fidelity & Systems
**Goal:** Achieve full multimedia parity and foundational hardware emulation for C64 compatibility.
**Theme:** C64 Compatibility & Multimedia

- [ ] **SID Audio:** Finalize `resid-fp` engine integration and implement a visual SID debugger (oscillators, envelopes, filters).
- [ ] **VIC-II Accuracy:** Implement cycle-exact rendering, sprite-to-sprite/background collisions, and light pen support.
- [ ] **6510/C64 System:** Implement the Processor Port ($00/$01), C64-style banking logic, and remaining undocumented opcodes (XAA, AXS, etc.).
- [ ] **Peripheral Support:** Implement basic 1541/CBM DOS emulation and IEC serial bus signals for disk drive communication.
- [ ] **Connectivity:** Implement remote hardware debugging (VICE Monitor interface, M65dbg Serial/Ethernet support).

---

## Milestone 2.2: Editable Idiom Library & Templating
**Goal:** Transform the static pattern library into a user-extensible system with configurable parameters.
**Theme:** Extensibility & Workflow

- [ ] **Dynamic Snippet Loading:** Transition from the hardcoded `g_snippets` array to a dynamic loader that scans a `patterns/` directory for individual `.asm` files.
- [ ] **Metadata Specification:** Implement a header-comment parser for snippet properties, supporting tags such as `; @name`, `; @category`, `; @processor`, `; @summary`, and `; @clobbers`.
- [ ] **Parameter Templating:** Introduce a templating syntax for Zero Page registers and constants (e.g., `!SRC_LO`, `!TEMP_VAR`). This allows snippets to define variable placeholders that the user can map to specific addresses or labels before insertion, overriding the snippet's internal defaults.
- [ ] **GUI Integrated Editor:** Enhance the Idiom Library pane with a built-in assembly editor and a "Snippet Settings" dialog for managing metadata and template overrides.
- [ ] **CRUD API:** Extend `sim_api.h` with functions for adding, updating, and deleting snippets, including disk persistence.

---

## Milestone 2.3: Zilog Z80 Core & Toolchain
**Goal:** Deliver high-fidelity Z80 instruction set emulation and assembler support.
**Theme:** Architecture Expansion.

- [ ] **Z80 Core Implementation:** Implement the Z80 instruction set, including all prefix-based opcodes (CB, DD, ED, FD) and the alternate register set.
- [ ] **Z80 Assembler Integration:** Update the toolchain to support Z80 assembly (e.g., via pasmo or z80asm).
- [ ] **Z80 Register File:** Implement the dynamic register metadata system for Z80's complex register set (AF, BC, DE, HL, IX, IY, etc.).

## Milestone 2.4: Common Z80 Devices & Systems
**Goal:** Implement the standard suite of Z80-family support chips and legacy system emulation.
**Theme:** Universal Retro-Debugging.

- [ ] **Z80 Peripheral Suite:** Implement emulation for the Z80 PIO (Parallel I/O), CTC (Counter/Timer), and SIO (Serial I/O).
- [ ] **CP/M Compatibility:** Implement BIOS/BDOS system call traps to allow running legacy CP/M software without full disk controller emulation.
- [ ] **ZX Spectrum ULA:** Implement basic video, interrupt timing, and keyboard matrix for the ZX Spectrum.
- [ ] **MSX VDP:** Implement basic video functionality for the MSX standard (TMS9918 compatible).
- [ ] **Hobbyist System Presets:** Add machine configurations for ZX Spectrum and MSX basics, including memory mapping and I/O port routing.

## Milestone 2.5: Architecture Expansion - AVR / Arduino [Commercial Target]
**Goal:** Implement the AVR architecture and common Arduino-compatible peripherals.
**Theme:** Embedded Systems & Education.

- [ ] **AVR Core Implementation:** Implement the ATmega instruction set and Harvard architecture memory model.
- [ ] **Arduino Peripheral Suite:** Implement GPIO ports, Timers/PWM, UART, and SPI/I2C (TWI) for standard Arduino compatibility.
- [ ] **Analog & UI Devices:** Implement ADC (Analog-to-Digital Converter) and support for HD44780 LCD character displays.

## Milestone 2.6: Architecture Expansion - ARM Cortex-M [Commercial Target]
**Goal:** Implement the 32-bit ARM RISC architecture for professional embedded development.
**Theme:** Professional Tooling & IoT.

- [ ] **ARM Core Implementation:** Implement the ARMv7-M (Cortex-M) instruction set and 32-bit register file.
- [ ] **ARM System Peripherals:** Implement the NVIC (Interrupt Controller), SysTick Timer, and basic DMA Controller.
- [ ] **Modern Connectivity:** Implement USB Device Controller and high-speed UART/GPIO.

---

## Future Roadmap (v2.x+)
- [ ] 8051 Architecture Support [Commercial Target]: Internal timers, UART, and banked memory.
- [ ] TI TMS320 DSP Support [Commercial Target]: McBSP, high-res PWM, and QEP.
- [ ] Vic3 Support
- [ ] Vic4 Support
- [ ] FAR: Support for additional CPU architectures (68000) [Commercial Target].
- [ ] ? Integration with TurboRascal toolchain.
- [ ] ? Web-based UI leveraging existing MCP/API infrastructure.

---

## Known Issues
- [ ] **CLI .cpu Collision:** Manual execution (dot prefix) fails for `.cpu` directives (e.g., `> .cpu 45gs02` errors) because it clashes with the simulator command and is passed incorrectly to the assembler.

## Finished

## Milestone 1.6: GUI Ease of Use, Minor Bug Fixes
**Goal:** Changes for the GUI interactions focused on ease of use. Minor bug fixes. 
**Theme:** Advanced UI & Refinement. 

- [X] **Memory Integrity:** Fix `far_pages` memory leaks during reloads and ensure correct `load_size` calculation for assembly programs.
- [X] **Interactive Graphics Editors:** Implement the interactive bitmap editor for the VIC-II Sprite pane (1bpp/2bpp modes) and create the new VIC-II Character Set Editor pane.
- [X] **Register Editing:** Replace modal dialogs with inline hex editing for registers.
- [X] **Memory View Refinement:** Add "Follow PC/SP" toggles, write-access highlighting, and inline cell editing.
- [X] **UX & Navigation:** Complete the "Go To Address" disassembly scrolling and ensure robust layout persistence for multi-instance panes.

## Milestone 1.5: wxWidget / Native Look and feel
**Goal:** Transition the user interface from a custom ImGui shell to a native OS-standard experience using wxWidgets.
**Theme:** Native Integration & Accessibility

The current GUI lives almost entirely in `src/gui/main.cpp` (~5876 lines) and relies on
Dear ImGui (docking branch, auto-fetched into `src/gui/imgui/`) with an SDL2 windowing
backend (`imgui_impl_sdl2.cpp`) and an OpenGL 3.3 rendering backend
(`imgui_impl_opengl3.cpp`). A header-only custom file dialog (`src/gui/imgui_filedlg.h`)
covers file open/save. All 21 panes and all menus are rendered imperatively inside a
single `main()` loop.

The wxWidgets migration replaces that stack with a native event-driven model while
preserving the simulator API boundary at `src/sim_api.h` / `src/sim_api.cpp` — the
same boundary used by the CLI and MCP server.

### Architectural Principles

- **All GUI code stays in `src/gui/`.**  New files should be split per pane/concern
  (e.g. `src/gui/pane_registers.cpp`, `src/gui/app.cpp`) rather than monolithic.
- **No GUI logic in libraries.** Any logic currently inlined in `main.cpp` that is
  simulator-domain (e.g. register diff tracking, disassembly formatting, history
  navigation) must be extracted into the appropriate `lib6502-*` layer and exposed via
  `src/sim_api.h` before being re-consumed by the wx panes. This keeps CLI and MCP
  parity achievable in Milestone 1.7.
- **The simulator API is the only interface.** Panes must not reach past `sim_api.h`
  into library internals. All state needed by a pane must be surfaced through API calls.
- **Drop SDL2 and OpenGL as build dependencies for the GUI** (except where
  `wxGLCanvas` replaces them for rendered content — see VIC-II / Profiler below).

### Phase A — Scaffold & Bootstrap

**Goal:** Get a bare wx app compiling and linking against `libsim6502.a`, replacing the
SDL2/OpenGL boilerplate in `main()`.

- [X] **Makefile: wxWidgets build target.**
  Add a `gui-wx` (or replace `gui`) Makefile target using `wx-config --cflags` and
  `wx-config --libs` in place of the current `SDL2_CFLAGS` / `SDL2_LIBS` / `GL_LIBS`
  variables. Remove the ImGui auto-clone recipe (`$(IMGUI_DIR)/imgui.h`) once migration
  is complete. Retain `libsim6502.a` as the only simulator dependency.

- [X] **`src/gui/app.h` / `src/gui/app.cpp` — `wxApp` subclass.**
  Implement `Sim6502App : public wxApp` with `OnInit()` / `OnExit()`. `OnInit()`
  creates the `MainFrame`, calls `Show()`, starts the simulation idle timer. Replaces
  the SDL `main()` entry point in `src/gui/main.cpp`.

- [X] **`src/gui/main_frame.h` / `src/gui/main_frame.cpp` — top-level `wxFrame`.**
  `MainFrame : public wxFrame` owns the `sim_session_t *` handle (currently
  `g_sim` static global in `main.cpp`). Responsible for:
  - calling `sim_open()` / `sim_close()` on construction/destruction.
  - holding the `wxAuiManager` instance.
  - dispatching the `wxTimer`-based simulation tick (replaces the ImGui frame loop's
    `execute_steps()` call).
  - owning the status bar (processor mode, cycle count, run state).

- [X] **Theme / DPI / font initialisation.**
  The current code in `main.cpp` reads `SIM6502_SCALE`, `GDK_SCALE`, `QT_SCALE_FACTOR`
  and GTK/GNOME/KDE dark-mode hints to set up ImGui's font and color scheme. Port this
  logic to `MainFrame::ApplyTheme()` using `wxSystemSettings::GetColour()` and
  `wxSystemAppearance` (wx 3.1.3+) for automatic OS dark-mode detection. Persist
  font-size and theme preference via `wxConfig` (replaces the custom INI handler in
  `main.cpp`).

### Phase B — Native Menus, Toolbar & Status Bar

**Goal:** Replace the ImGui menu bar (currently rendered at `main.cpp:5573`) with a
full native menu bar, toolbar, and status bar.

- [X] **`src/gui/main_frame_menus.cpp` — menu bar construction.**
  Build `wxMenuBar` from the existing five top-level menus:
  - *File*: New Project, Load (Ctrl+L), Browse to Load, Save Binary/PRG, Quit (Alt+F4).
  - *Simulation*: Step Into (F7), Step Over (F8), Run (F5), Pause (F6), Reset (Ctrl+R),
    Toggle Breakpoint (F9), Step Back (Shift+F7), Step Forward (Shift+F8),
    Reverse Continue (Ctrl+Shift+R), Throttle Speed.
  - *Machine*: Add Optional Device, Processor selector, Machine type selector,
    SID Debugger, Audio Mixer.
  - *View*: one item per pane (see Phase C), Memory sub-menu (up to 4 views +
    "Add Memory View"), Go to Address (Ctrl+G), Font Size sub-menu, Theme sub-menu,
    Layout Presets (Save / Reset).
  - *Help*: About.

  Bind each item to a `wxCommandEvent` handler. Accelerator table replaces ImGui's
  inline `IsKeyPressed()` polling scattered through `main.cpp`.

- [X] **Native toolbar.**
  `wxAuiToolBar` with icons for: Load, Step Into, Step Over, Run, Pause, Reset,
  Toggle Breakpoint. Add a processor/machine combo selector to the toolbar's right end
  (currently a dropdown in the Machine menu rendered inside the ImGui menu bar area).

- [X] **Status bar.**
  Three-field `wxStatusBar`: [Run State | Processor | Cycles]. Update from the
  simulation tick timer.

### Phase C — AUI Docking & Pane Shell

**Goal:** Replace ImGui's docking system with `wxAUI`, giving each of the 21 panes
a proper `wxPanel`-derived host, and restoring docked layout from `wxConfig`.

- [X] **`wxAuiManager` integration in `MainFrame`.**
  Call `m_aui.SetManagedWindow(this)` in `MainFrame::OnInit()`. Register each pane
  panel with `wxAuiPaneInfo` (caption, dock direction, initial size, floatable,
  closable). Persist layout via `m_aui.SavePerspective()` / `LoadPerspective()` stored
  in `wxConfig`. Replaces ImGui's `DockSpace` and per-pane `SetNextWindowSize` /
  `ImGui::Begin()` calls.

- [X] **`src/gui/pane_base.h` — `SimPane` base class.**
  `SimPane : public wxPanel` takes a `sim_session_t *` reference and provides:
  - `virtual void Refresh(const SimSnapshot &snap)` — called every tick with current
    CPU/memory state (struct to be defined; wraps the existing `sim_get_cpu()` /
    `sim_get_memory()` API calls).
  - `virtual wxString GetPaneTitle() const = 0`.
  All 21 pane classes derive from `SimPane`.

- [X] **View menu ↔ pane visibility binding.**
  Each View menu item calls `m_aui.GetPane(panel).Show(visible)` and
  `m_aui.Update()`. Replaces the per-pane `show_*` boolean globals in `main.cpp`.

- [X] **Layout Preset save/restore.**
  Implement the "Save Layout Preset" and preset-load items using named perspectives
  in `wxConfig`, replacing the ImGui modal "Save Layout Preset##lsp" and the
  `g_layout_presets` vector in `main.cpp`.

### Phase D — Core Text Panes (native `wxListCtrl` / `wxTextCtrl`)

**Goal:** Re-implement the four primary debugger panes using native wx controls,
improving accessibility, keyboard navigation, and OS clipboard integration.

- [X] **`src/gui/pane_registers.cpp` — Registers pane.**
  Replace `draw_pane_registers()`. Use a `wxListCtrl` (report view) with columns:
  Register | Value | Previous | Flags. Highlight changed registers with a coloured
  background (currently `text_changed` colour in `AppColors`). Inline editing via
  `wxListCtrl::EditLabel` or a secondary `wxTextCtrl` for register write-back through
  `sim_set_register()`.

- [X] **`src/gui/pane_disassembly.cpp` — Disassembly pane.**
  Replace `draw_pane_disassembly()`. Virtual `wxListCtrl` (owner-drawn rows) for
  efficient rendering of large address ranges without pre-allocating all rows. Columns:
  Breakpoint gutter (toggle on click → `sim_break_set()`) | Address | Bytes | Mnemonic
  | Operand | Cycles | Symbol. Auto-scroll to PC on each tick. Syntax colouring for
  mnemonics (currently `AppColors::mnemonic`) and symbol labels. "Go to Address" via
  `wxTextEntryDialog` replacing the ImGui popup "Go to Address##goto".

- [X] **`src/gui/pane_memory.cpp` — Memory pane (up to 4 instances).**
  Replace `draw_pane_memory()`. Implement a hex-editor-style `wxScrolledWindow` with
  owner-drawn cells: 16 bytes/row, address column, hex columns, ASCII column. Support
  up to 4 independent instances (each with its own base address) matching current
  `g_mem_view[]` array. Byte editing writes through `sim_mem_write_byte()`. Address
  search via toolbar `wxTextCtrl`.

- [X] **`src/gui/pane_console.cpp` — Console pane.**
  Replace `draw_pane_console()`. Pair a read-only `wxTextCtrl` (output log, coloured
  by `wxTextAttr`) with an input `wxTextCtrl` for command entry. Command history
  navigation on Up/Down keys (currently `g_cli_history` in `main.cpp`). Pass entered
  commands to the existing CLI interpreter via `sim_execute_command()` (or equivalent
  API to be added to `sim_api.h` if not yet present). Coloured output categories: ok,
  error, warning, info (maps from `AppColors::text_ok/err/warn`).

### Phase E — Debugging Panes

- [X] **`src/gui/pane_breakpoints.cpp`** — Replace `draw_pane_breakpoints()`.
  `wxListCtrl` with columns: # | Address | Symbol | Type | Condition. Toolbar buttons:
  Add (→ `wxTextEntryDialog`), Delete, Clear All. Data from `sim_break_*()` API.

- [X] **`src/gui/pane_trace.cpp`** — Replace `draw_pane_trace()`.
  `wxNotebook` with two pages: *Live Log* (auto-scrolling `wxTextCtrl`) and
  *Run Trace* (virtual `wxListCtrl` over `sim_trace_*()` data). The trace entry struct
  `sim_trace_entry_t` is defined in `src/lib6502-debug/debug_types.h`.

- [X] **`src/gui/pane_stack.cpp`** — Replace `draw_pane_stack()`.
  `wxListCtrl` showing stack depth, return addresses, and resolved symbols. Data from
  `sim_get_cpu()` SP field plus `sim_mem_read_byte()` across the stack page.

- [X] **`src/gui/pane_watches.cpp`** — Replace `draw_pane_watches()`.
  `wxListCtrl` with 16 user-defined watch slots. Columns: Label | Address | Value
  (hex) | Value (dec) | Value (ASCII). Add/remove via context menu. Replaces
  `g_watches[]` array and inline edit widgets in `main.cpp`.

- [X] **`src/gui/pane_snap_diff.cpp`** — Replace `draw_pane_snap_diff()`.
  `wxListCtrl` showing `sim_diff_entry_t` rows from `sim_snapshot_diff()`. Columns:
  Address Range | Before | After | Writer PC. Snapshot/clear controls in pane toolbar.
  `sim_diff_entry_t` is defined in `src/lib6502-debug/debug_types.h`.

### Phase F — Reference & Analysis Panes

- [X] **`src/gui/pane_iref.cpp`** — Replace `draw_pane_iref()`.
  `wxSplitterWindow`: top half is a `wxListCtrl` of all opcodes (mnemonic, mode,
  opcode byte, cycles); bottom half is a `wxTextCtrl` showing detail for the selected
  opcode. Filter toolbar `wxTextCtrl` for mnemonic search.

- [X] **`src/gui/pane_symbols.cpp`** — Replace `draw_pane_symbols()`.
  Virtual `wxListCtrl`: Name | Address | Type | Source. Filter by prefix. Load
  external `.sym` via `wxFileDialog` + `sim_sym_load()`. Save via `sim_sym_save()`.

- [X] **`src/gui/pane_source.cpp`** — Replace `draw_pane_source()`.
  `wxStyledTextCtrl` (Scintilla) for syntax-highlighted source display, search
  highlight, and PC-sync scrolling. Read-only; source text from `sim_get_source_text()`
  (API to be confirmed/added). Line-to-address mapping from symbol data.

- [X] **`src/gui/pane_profiler.cpp`** — Replace `draw_pane_profiler()`.
  The heatmap is a 256×256 RGB texture computed from `sim_profiler_get_hits()`.
  Render via `wxGLCanvas` (same approach as VIC-II, see Phase G) or as a pre-rendered
  `wxBitmap` updated each tick (simpler, acceptable for 64KB address space).
  Tooltip on hover showing hit count for the pointed address.

- [X] **`src/gui/pane_test_runner.cpp`** — Replace `draw_pane_test_runner()`.
  `wxSplitterWindow`: file tree (`wxTreeCtrl`) on left; result detail (`wxTextCtrl`)
  on right. "Run All" / "Run Selected" toolbar buttons. Launch test process via
  `wxProcess` + `wxExecute()` replacing the current `popen()` / thread-based runner
  in `main.cpp`.

- [X] **`src/gui/pane_devices.cpp`** — Replace `draw_pane_devices()`.
  `wxPropertyGrid` listing device registers and configuration fields. Update on each
  tick from `sim_get_device_state()` (API to be confirmed).

- [X] **`src/gui/pane_patterns.cpp`** — Replace `draw_pane_patterns()`.
  `wxSplitterWindow`: `wxListBox` of snippet names (from `snippet_find()` in
  `src/lib6502-toolchain/patterns.h`) on left; `wxStyledTextCtrl` preview on right.
  "Insert" button copies snippet text to clipboard. Filter `wxTextCtrl` for name search.

### Phase G — Hardware / High-Performance Rendering Panes

These panes render pixel data and must use `wxGLCanvas` for performance. The OpenGL
texture update logic currently embedded in `main.cpp` (heatmap texture at line ~4200,
VIC-II screen texture at line ~4600, sprite textures at line ~4700) should be
encapsulated inside the respective panel classes.

- [X] **`src/gui/pane_vic_screen.cpp`** — Replace `draw_pane_vic_screen()`.
  `wxGLCanvas` sized to the 384×272 VIC-II output. Each tick, retrieve the pixel
  buffer via `sim_vic_get_framebuffer()` (API to be confirmed), upload to an OpenGL
  texture, blit to canvas. Aspect-ratio-correct scaling on resize.

- [X] **`src/gui/pane_vic_sprites.cpp`** — Replace `draw_pane_vic_sprites()`.
  8 sub-panels (or a `wxNotebook` of 8 pages), each a `wxGLCanvas` for one sprite's
  24×21 RGBA texture. Current textures: `g_sprite_tex[8]` in `main.cpp`.

- [X] **`src/gui/pane_vic_regs.cpp`** — Replace `draw_pane_vic_regs()`.
  `wxPropertyGrid` for all VIC-II control registers. Editable values write back
  through `sim_mem_write_byte()` at VIC-II register addresses.

- [X] **`src/gui/pane_sid_debugger.cpp`** — Replace `draw_pane_sid_debugger()`.
  `wxPanel` with a grid of SID register read-outs. Voice oscillator visualisation can
  use a `wxGLCanvas` or a `wxClientDC` owner-drawn curve plot.

- [X] **`src/gui/pane_audio_mixer.cpp`** — Replace `draw_pane_audio_mixer()`.
  `wxPanel` with `wxSlider` controls for volume, voice mix. Label each control with
  register name and current hex value.

### Phase H — Native Dialogs

Replace all ImGui `BeginPopupModal()` and `imgui_filedlg.h` usage with native wx dialogs.

- [X] **File open/save → `wxFileDialog`.**
  Current modes: `FILEDLG_OPEN_FILE`, `FILEDLG_OPEN_SYM`, `FILEDLG_SAVE_BIN` in
  `main.cpp`. Replace with `wxFileDialog` (style `wxFD_OPEN` / `wxFD_SAVE`). File
  type filters: `*.asm;*.prg;*.bin` (load), `*.sym` (symbol), `*.bin;*.prg` (save).

- [X] **"Assembly Error" modal → `wxMessageBox()`.**
  Currently `ImGui::BeginPopupModal("Assembly Error")`. Replace with
  `wxMessageBox(errorText, "Assembly Error", wxOK | wxICON_ERROR)`.

- [X] **"Load Binary##binload" → custom `wxDialog`.**
  Address entry field (`wxTextCtrl`, hex validated) plus Load/Cancel. Load calls
  `sim_load_bin()`. Port from `main.cpp modal of same name.

- [X] **"Add Optional Device" → `wxSingleChoiceDialog`.**
  Lists available devices (SID, VIC2, CIA, etc.). Selection adds device via
  `sim_add_device()` (or equivalent API).

- [X] **"Save Binary##binsave" → custom `wxDialog`.**
  Start address, end address, filename fields. Calls `sim_save_bin()`.

- [X] **"New Project Wizard" → multi-page `wxWizard`.**
  Port the multi-step project creation flow from `main.cpp`. Steps: project name /
  directory, processor type, machine preset, template snippet. Writes `project.toml`
  (or equivalent) via `src/lib6502-toolchain/project_manager.h`.

- [X] **"Go to Address##goto" → `wxTextEntryDialog`.**
  Hex address input; on OK, scrolls the Disassembly or Memory pane to that address.

- [X] **"Save Layout Preset##lsp" → `wxTextEntryDialog`.**
  Preset name input; saves `wxAuiManager::SavePerspective()` under that name in
  `wxConfig`.

### Phase I — Cleanup & Build Finalisation

- [X] **Remove ImGui sources and build rules.**
  Delete `src/gui/imgui/` directory and all ImGui-related Makefile variables
  (`IMGUI_DIR`, `IMGUI_BACK`, `IMGUI_SRCS`, `IMGUI_OBJS`, the auto-clone recipe).
  Remove `src/gui/imgui_filedlg.h`.

- [X] **Remove SDL2 / OpenGL from GUI build.**
  Remove `SDL2_CFLAGS`, `SDL2_LIBS`, `GL_LIBS` from the `gui` Makefile target
  (retain only for `wxGLCanvas` linkage if OpenGL is still needed; `wx-config --libs`
  covers this when `wxGLCanvas` is enabled).

- [X] **Validate `src/gui/main.cpp` is fully replaced.**
  The original monolithic file should be deleted once all 21 panes and the app
  bootstrap are accounted for in the new split files.

- [X] **API surface review.**
  Any simulator state currently accessed via static globals or internal casts in
  `main.cpp` must be surfaced through new or existing `sim_api.h` calls. Document any
  additions to `sim_api.h` and corresponding `sim_api.cpp` implementations.

- [X] **`make test` regression.**
  The CLI (`sim6502`) must continue to pass `make test` (runs `tools/run_tests.py`) and
  `make unit-test` without change. The GUI build must not alter `libsim6502.a` 
  contents or CLI linkage.

### Phase J — Cleanup and rename sim6502-gui-wx to sim6502-gui

- [X] **Remove Existing sim6502-gui source**
  The monolithic ImGui-based `src/gui/main.cpp` has been removed and replaced by a modular wxWidgets structure.

- [X] **Rename sim6502-gui-wx to be sim6502-gui**
  The Makefile now builds the wxWidgets frontend as the primary `sim6502-gui` target.

## Milestone 1.1: Unit Testing
**Goal:** Implement unit tests for a wide swatch of the application.
**Theme:** Testing & Reliability.

- [X] **Core Tests:** Implemented unit tests for CPU opcodes, arithmetic, and addressing modes.
- [X] **Device Tests:** Added verification for CIA timers and VIC-II register behaviors.
- [X] **Debug Tests:** Validated breakpoint and trace logic.
- [X] **Integration:** Established a test suite for end-to-end simulation flows.

## Milestone 1.0: Stability & Core Fidelity
**Goal:** Establish a rock-solid core for the simulator with a focus on CPU fidelity, toolchain reliability, and a polished user experience.

- [X] **Interactive Debugging:**
  - **Manual Execution:** Support direct instruction execution from the `IDLE` state (e.g., `.LDA #$01` entered in console).
  - **Step Variants (CLI):** Implement `next` (step over) and `finish` (step out) for the CLI; hook up `sb`/`sf` history navigation.
  - **Disassembly:** Implement functional disassembly in `sim_disassemble_one` even in `IDLE` or error states.
- [X] **Error Handling & UX:** (Gui, CLI, MCP) Add (GUI: modal) feedback for assembly failures and improve error surfacing at normal verbosity levels.
- [X] **Test Coverage:** Fix failing VIC2/sprite pattern tests (18 currently failing in `make test` pattern suite).
- [X] **CPU Logic & Flags:** Correct Interrupt B-Flag behavior (IRQ/NMI vs BRK/PHP).
- [x] **BRK Handling:** `BRK` now fully executes (pushes PC+2 and P to stack, sets I, loads IRQ vector from `$FFFE/$FFFF`). Programs exit the simulator via `RTS` with an empty stack (as if the simulator had JSR'd into the program).
- [x] **RMW + ALU Opcodes:** `SLO`, `RLA`, `SRE`, `RRA`, `DCP` (as `DCM`), `ISC` (as `INS`).
- [x] **Load/Store Opcodes:** `LAX`, `SAX`.
- [x] **Immediate Opcodes:** `ANC`, `ALR`, `ARR`, `ASR`.
- [x] **CIA Timers:** Implement two 16-bit programmable interval timers (Timer A and Timer B).
- [x] **CIA Keyboard:** Emulate the 8x8 keyboard matrix for input.
- [x] **CIA Interrupts:** Generate IRQs for timers, keyboard, and other CIA events.
- [x] **VIC-II Banking:** Control the 16KB window seen by the VIC-II via bits 0-1 of Port A ($DD00).
- [x] **NMI Generation:** Support Non-Maskable Interrupts for timers.
- [x] **MCP Concurrency:** Unique temporary filenames to prevent race conditions.
- [x] **MCP Security:** Sanitize input to ensure no control characters or shell-escape sequences.
- [x] **MCP Compatibility:** Platform-agnostic path handling for screen/bitmap saves.
- [x] **MCP Lifecycle:** Unified entry points and configurable paths via environment variables.
- [x] **MCP Optimization:** Persistent simulator process for routine validation.
- [x] **MCP Protocol:** Updated `assemble` tool handler to use JSON mode (`-J`).
- [x] **MCP Tools:** Added `shutdown_simulator` tool for manual resource management.
