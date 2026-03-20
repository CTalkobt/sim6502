# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased] - 2026-03-19

### Added

#### New GUI Panes
- **Source Viewer pane** (`pane_source`): displays the assembled `.asm` source file with syntax highlighting, a PC arrow marker, and a breakpoint click margin.
  - Ctrl+F find bar with forward/backward search and wrap-around; Enter advances, Shift+Enter goes back, Escape closes.
  - Toolbar Reload button refreshes the file from disk without re-loading the whole session.
  - Pane title in toolbar updates to the loaded filename (e.g. `45gs02_factorial.asm`); AUI pane caption also updated.
  - Right-click context menu: **Go to Address in Disassembly** navigates the Disassembly pane to the instruction at the clicked source line; **Go to Address in Memory View** navigates the Memory pane to the instruction's *operand* address (e.g. right-clicking `STA ITER_COUNT` navigates to `$0012`, not the opcode address). Both items are disabled when the line has no mapped address.
  - PC arrow auto-scrolls to follow execution on every refresh frame, not just on step events.
- **Instruction Reference pane** (`pane_iref`): lists every opcode with mnemonic, addressing modes, flags affected, and a short description; searches and filters by mnemonic or description. Built from a new `instruction_descriptions.h` table (143-entry static array). `sim_api` additions expose instruction metadata to the pane.
- **Snapshot Diff pane** (`pane_snap_diff`): shows a table of memory locations that changed between the last `snap` and the current state, with before/after values and the PC of the writing instruction. Integrates with `debug_context` snapshot API additions (`sim_snapshot_start`, `sim_snapshot_diff`, `sim_snapshot_clear`).
- **VIC-II Character Editor pane** (`pane_vic_char_editor`): OpenGL-rendered character-set viewer/editor. Displays all 256 characters from the VIC-II character ROM in a 16x16 grid; clicking a character opens a zoomed pixel editor. Loads the default PET upper-case charset from `presets/default-pet-upper.bin` (added alongside).

#### Pane Improvements
- **Register pane** (`pane_registers`): added **Prev** column showing the register value one instruction ago (rotates only on cycle changes, not every refresh); added **Flags/Bits** column showing per-bit breakdown of status flags; inline hex editing with ghosting suppression during active edit; immediate refresh after a manual edit.
- **Disassembly pane** (`pane_disassembly`): major rewrite — instruction-boundary-aware row layout (replaces fixed byte-per-row); address search/jump bar; filter by mnemonic; `NavigateDisassembly(addr)` method on `MainFrame` allows other panes and context menus to scroll here programmatically. Added `sim_disassemble_entry` to `sim_api` exposing a `sim_disasm_entry_t` with `has_target` / `target_addr` for resolved operand addresses; used by Watch List, Source Viewer, and context menus.
- **Memory pane** (`pane_memory`): address search bar; follow-PC and follow-SP modes that auto-scroll on each step; `NavigateMemory(addr)` on `MainFrame` (deferred scroll via `CallAfter` so hidden panes lay out before `EnsureVisible` runs). Added `sim_disassemble_entry` integration for correct operand-address navigation.
- **Trace Log pane** (`pane_trace`): major rewrite — virtual list rendering for large histories, column sort, filter by mnemonic or address range, export to CSV; `NavigateFromTrace` links trace entries to the Disassembly and Memory panes. Trace follow mode auto-scrolls to the newest entry during live execution.
- **Watch List pane** (`pane_watches`): major rewrite — inline add/edit/delete rows; display modes (hex, decimal, binary, ASCII); expression watches (register references); auto-refresh on step; integrates `sim_disasm_entry_t` for memory watches that show the current operand target.
- **Symbol pane** (`pane_symbols`): filter bar; copy address/name to clipboard; double-click navigates Disassembly pane; added `sim_symbol_count`, `sim_symbol_get_by_index`, `sim_symbol_find_by_name` to `sim_api`; underlying `symbols.cpp` additions for indexed and name-based lookup.
- **Stack pane** (`pane_stack`): shows full stack contents with hex and ASCII columns; highlights the current SP row; return-address annotation where recognizable; copy row to clipboard.
- **Breakpoints pane** (`pane_breakpoints`): add/delete breakpoints by address or symbol; enable/disable toggle; condition display; integrates `sim_api` breakpoint enumeration (`sim_breakpoint_count`, `sim_breakpoint_get`).
- **Console pane** (`pane_console`): full interactive CLI inside the GUI — command history (Up/Down), auto-complete, `mem` output column alignment matches CLI; `cliIsInteractiveMode()` helper suppresses GUI-irrelevant CLI options from `help` output; `quit`/`exit` intercepted with friendly message; `help <command>` renders per-command help via the CLI registry.
- **Profiler pane** (`pane_profiler`): major rewrite — hotspot table sorted by cycle count or call count, per-address annotation, live update during execution, reset button.
- **Audio Mixer pane** (`pane_audio_mixer`): per-voice volume sliders, mute toggles, master volume; SID register live readout alongside mixer controls.
- **I/O Devices pane** (`pane_devices`): register-by-register view of CIA1, CIA2, SID, and MEGA65 I/O; read-only monitor with live refresh.
- **Idiom Library pane** (`pane_patterns`): displays all pattern library entries with copy-to-clipboard; search/filter by name; processor variant selector (6502/65c02/45gs02).
- **VIC-II Registers pane** (`pane_vic_regs`): added full register name and bit-field annotations alongside raw hex values.
- **VIC-II Sprites pane**: added unit tests for sprite data and DMA state (`tests/unit/test_vic2_sprites.cpp`).

#### Application Shell
- **Pane close sync**: closing a pane via its `x` icon now unchecks the corresponding View menu item (`MainFrame::OnPaneClose` handler added).
- **Menu restructure** (`main_frame_menus.cpp`): View menu reorganised into sub-menus by category (Debug, VIC-II, Audio); menu items carry tooltips.
- **`SimPane` base** (`pane_base.h`): added `GetPaneTitle()` / `GetPaneName()` virtual helpers; `MainFrame::UpdatePaneCaption` uses these to keep AUI tab captions in sync with loaded content.

#### Simulator / Toolchain
- **KickAssembler `-debugdump`**: assembler invocation now includes `-debugdump`, producing a `.dbg` (C64debugger XML) alongside every `.prg`. Provides a complete address-to-source-line map used by the Source Viewer and context menus.
- **`.dbg` parser** (`source_map_load_kickass_dbg` in `list_parser.cpp`): parses KickAssembler's C64debugger XML format into `source_map_t`, handling multi-source projects via the `<Sources>` index. Paths ending in `.asm.tmp` are transparently rewritten to `.asm`.
- **Re-assembly on missing debug info**: `load_toolchain_bundle` re-runs KickAssembler when a `.prg` already exists but its companion `.dbg` is absent, ensuring debug info is always available without forcing a full rebuild every load.

### Fixed
- **Source Viewer PC arrow off-by-one**: `preprocess_asm_pseudoops` was inserting an extra `.print "SIM_CPU:..."` line after every `.cpu`/`.processor` directive, shifting all subsequent `.dbg` line numbers by +1. CPU type is now detected by directly scanning the original `.asm` before assembly, eliminating the extra-line injection.
- **Source map path mismatch**: context-menu address lookups now fall back to a line-number-only match when the stored absolute path does not match `m_loadedPath` (which may be relative from `sim_get_filename`). Fixes grayed-out context menu items for pre-built `.prg` files loaded without an explicit path.
- **Memory View navigation on hidden pane**: `MainFrame::NavigateMemory` previously called `ScrollTo` before the pane was visible, making `EnsureVisible` a no-op. The pane is now shown and `m_aui.Update()` called first; `ScrollTo` is deferred via `CallAfter` so the list has its final size before the scroll offset is calculated.
- **Memory View navigates to operand address, not PC**: "Go to Address in Memory View" now disassembles the instruction at the clicked source line and uses `sim_disasm_entry_t.target_addr` when `has_target` is true (e.g. `STA $12` → `$0012`). Immediate and implied instructions fall back to the PC address.
- **Disassembly pane row layout**: instruction-boundary tracking replaces the previous fixed byte-per-row display, so multi-byte instructions (especially 45GS02 with EOM prefix) no longer show operand bytes as separate instructions.

## [Unreleased] - 2026-03-16

### Added
- GUI console pane: `quit`/`exit` commands are now intercepted and display "Use File > Quit to exit." instead of silently doing nothing.
- `cliIsInteractiveMode()` helper in `commands.cpp` — returns true when running in the text CLI (no log callback set), false when running inside the GUI console pane. Used to suppress GUI-irrelevant options from help output.

### Fixed
- `help <command>` in the GUI console pane now correctly renders the command's help text via the registered `CLICommand::render_help()` path. Previously the command could abort due to the help dispatch not routing through the registry.
- `quit` no longer appears in the `help` command listing when running inside the GUI console pane.


- `sim6502-gui` now accepts the same key command-line options as `sim6502`: `-p`/`--processor`, `-t`/`--target`, `-b`/`--break`, `-L`/`--limit`, `-S`/`--speed`, and `--debug`.
- `-S` (speed scale) in the GUI drives `sim_step_cycles()` per timer tick, giving accurate cycle-budget execution at the requested clock rate (1.0 = C64 PAL 985 kHz, 0.0 = unlimited).
- `-L` (cycle limit) in the GUI pauses the running simulation once `cpu->cycles` reaches the specified count.
- New `examples/45gs02_factorial.asm` — computes N! (N ≤ 12) using the MEGA65 hardware multiplier, stack-pushed factors, and the 32-bit Q register; includes EXPECT comment verified at N=12 (12! = `$1C8CFC00`).

### Fixed
- `tools/run_tests.py`: Tests without an `// EXPECT:` clause now skip simulator execution entirely and pass on assembly success alone. Previously the simulator was always launched, causing the `all_65c02.asm` and `all_65ce02.asm` coverage files to time out or crash (the 65CE02's 16-bit stack pointer would wrap into MEGA65 I/O-mapped memory during the unintended BRK loop, triggering I/O handlers and eventually a segfault).
- Removed `tests/code/sid_test.asm` — the file contained an infinite `jmp loop` with no `EXPECT:` clause or termination condition, making it meaningless as a test.
- `cli/main.cpp`: `-p <processor>` flag now sets `machine_type` alongside `cpu_type` (e.g. `-p 45gs02` now correctly activates the MEGA65 I/O subsystem). Previously `machine_type` was left at its `MACHINE_C64` default, so the MEGA65 math coprocessor was never registered and writes to `$D770–$D777` silently fell through without triggering multiplication.
- `mega65_io.cpp`: MEGA65 math coprocessor now implements the correct 32-bit register layout — MULTINA (`$D770–$D773`), MULTINB (`$D774–$D777`), MULTOUT (`$D778–$D77F`) — matching the hardware specification. The previous implementation used a 16-bit layout with MULTINA at `$D770–$D771` and erroneously placed MULTINB at `$D772–$D773`.
- `patterns.cpp` (`mul8_mega65`): Fixed label addresses for MULTINB — was incorrectly using `$D772–$D773` (the upper half of MULTINA) instead of `$D774–$D777`. Result was always zero.

## [Unreleased] - 2026-03-15

### Added
- Support for launching the GUI with a filename argument (e.g., `./sim6502-gui examples/45gs02_test.asm`).
- `sim_set_reg_value` to `sim_api.h` and `sim_api.cpp` for 16-bit register support.
- Inline hex editing for the Registers pane in the GUI, replacing modal dialogs.
- Unit tests for `sim_set_reg_value` in `tests/unit/test_sim_api.cpp`.
- Naming convention rule to `AGENTS.md` (CamelCase for new naming).
- Infrastructure for 45GS02 `EOM` (0xEA) prefix handling in dispatch and disassembly.

### Fixed
- 45GS02 disassembly bug where operand bytes were incorrectly decoded as independent instructions (e.g., `NEG`).
- Infinite loop regression in standalone `EOM` (0xEA) instruction handling.
- Disassembly pane now correctly tracks instruction boundaries instead of a fixed byte-per-row layout.
- Registers pane "Prev" column now correctly shows values from the previous instruction by only rotating states on cycle changes.
- Corrected 16-bit Stack Pointer (SP) display and editing in the GUI.
- Fixed "ghosting" in register editor by suppressing underlying text during active edits.
- Suppressed display of the 'Prev' value for a register while it's being edited for a cleaner input experience.
- Fixed immediate refresh of manually edited register values in the GUI.

## [Unreleased] - 2026-03-14

- Conversion to utilize wxWidgets
