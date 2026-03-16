# Changelog

All notable changes to this project will be documented in this file.

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
