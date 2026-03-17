# Changelog

All notable changes to this project will be documented in this file.

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
