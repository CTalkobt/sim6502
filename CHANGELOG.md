# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased] - 2026-03-16

### Added
- `sim6502-gui` now accepts the same key command-line options as `sim6502`: `-p`/`--processor`, `-t`/`--target`, `-b`/`--break`, `-L`/`--limit`, `-S`/`--speed`, and `--debug`.
- `-S` (speed scale) in the GUI drives `sim_step_cycles()` per timer tick, giving accurate cycle-budget execution at the requested clock rate (1.0 = C64 PAL 985 kHz, 0.0 = unlimited).
- `-L` (cycle limit) in the GUI pauses the running simulation once `cpu->cycles` reaches the specified count.
- New `examples/45gs02_factorial.asm` — computes N! (N ≤ 12) using the MEGA65 hardware multiplier, stack-pushed factors, and the 32-bit Q register; includes EXPECT comment verified at N=12 (12! = `$1C8CFC00`).

### Fixed
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
