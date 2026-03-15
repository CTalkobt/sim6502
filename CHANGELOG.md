# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased] - 2026-03-15

### Added
- `sim_set_reg_value` to `sim_api.h` and `sim_api.cpp` for 16-bit register support.
- Inline hex editing for the Registers pane in the GUI, replacing modal dialogs.
- Unit tests for `sim_set_reg_value` in `tests/unit/test_sim_api.cpp`.
- Naming convention rule to `AGENTS.md` (CamelCase for new naming).

### Fixed
- Registers pane "Prev" column now correctly shows values from the previous instruction by only rotating states on cycle changes.
- Corrected 16-bit Stack Pointer (SP) display and editing in the GUI.
- Fixed "ghosting" in register editor by suppressing underlying text during active edits.
- Suppressed display of the 'Prev' value for a register while it's being edited for a cleaner input experience.
- Fixed immediate refresh of manually edited register values in the GUI.

## [Unreleased] - 2026-03-14

- Conversion to utilize wxWidgets
