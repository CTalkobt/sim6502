# Release and Milestone Schedule

This document outlines the release schedule for the 6502 Simulator, prioritizing stability, usability, and core features for the initial 1.0 release, followed by themed updates for advanced functionality.

---

## Milestone 1.5: wxWidget / Native Look and feel
**Goal:** Transition the user interface from a custom ImGui shell to a native OS-standard experience using wxWidgets.
**Theme:** Native Integration & Accessibility

- [ ] **wxWidgets Framework:** Scaffold the `wxApp` and `wxFrame` architecture, replacing the SDL2/OpenGL boilerplate with a `wxWidgets` event-driven model.
- [ ] **Native Menus & Toolbars:** Port the File, Run, Hardware, and View menus to native OS-standard menu bars and implement a native high-DPI toolbar.
- [ ] **AUI Docking System:** Implement `wxAUI` for docking management of all debugger panes (Registers, Disassembly, Memory, Console) to support native window snapping and layout persistence.
- [ ] **Core Pane Migration:** Re-implement the primary text-based panes (Registers, Memory, Watch) using native controls for better accessibility and OS integration.
- [ ] **High-Performance Rendering:** Integrate a `wxGLCanvas` for the VIC-II display and Profiler heatmap to maintain simulation performance within the native UI.
- [ ] **Native Dialogs & Feedback:** Replace all ImGui-based modals and the file dialog with native `wxFileDialog`, `wxPropertyGrid`, and standard OS message boxes.

---
## Milestone 1.6: GUI Ease of Use, Minor Bug Fixes
**Goal:** Changes for the GUI interactions focused on ease of use. Minor bug fixes. 
**Theme:** Advanced UI & Refinement. 

- [X] **Memory Integrity:** Fix `far_pages` memory leaks during reloads and ensure correct `load_size` calculation for assembly programs.

---

## Milestone 1.7: Ensure GUI, MCP and CLI are equally functional. 
**Goal:** Get the application into a fully functioning usable state for all 3 states of GUI, MCP and CLI. 
**Theme:** Corrections & Refinement. 

- [X] **Unified Logic:** Consolidate execution logic between GUI and CLI to ensure consistent behavior for all step command variants.
- [ ] **JSON Consistency:** Add `g_json_mode` (`-J`) support to `disasm`, `asm`, `jump`, `write`, `set`, `flag`, `bload`, and `bsave` for robust front-end integration.
- [ ] **Command Migration:** Move remaining CLI handlers from `commands.cpp` to the modular `CommandRegistry` architecture.
- [ ] **Command Standardization:** Rename legacy CLI commands (e.g., `flag` -> `set_flag`, `bload`/`bsave` -> `load`/`save`) to follow standard debugger conventions.
- [ ] **Breakpoint Management:** Implement `list` and `clear` CLI command handlers to expose existing API functionality across all interfaces.
- [ ] **Terminology Alignment:** Rename "patterns" to "Idioms" and "templates" to "Project Templates" across the codebase and documentation.

---

## Milestone 1.8: Architectural Integrity & MCP Robustness
**Goal:** Finalize the library extraction and ensure the MCP server meets modern production standards.
**Theme:** Infrastructure & Decoupling.

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

## Future Roadmap (v1.x+)
- [ ] Support for additional CPU architectures (Z80, 68000).
- [ ] Integration with TurboRascal toolchain.
- [ ] Web-based UI leveraging existing MCP/API infrastructure.

---

## Known Issues
- [ ] **CLI .cpu Collision:** Manual execution (dot prefix) fails for `.cpu` directives (e.g., `> .cpu 45gs02` errors) because it clashes with the simulator command and is passed incorrectly to the assembler.

## Finished

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
