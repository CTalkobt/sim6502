# 6502 Simulator MCP Server

A [Model Context Protocol](https://modelcontextprotocol.io/) (MCP) server that exposes
the `sim6502` simulator to AI coding assistants. Supports 6502, 65C02, 65CE02, and
45GS02 (MEGA65) processor variants.

---

## Prerequisites

1. Build the simulator binary from the project root:
   ```bash
   make
   ```
2. Node.js 18 or later
3. Install MCP server dependencies:
   ```bash
   cd mcp-server && npm install
   ```

---

## Setup by Tool

### Claude Code (Claude CLI)

**Option A — Project-scoped** (recommended)

Copy the template and fill in your absolute path:
```bash
cp mcp-server/.mcp.json.template .mcp.json
# Edit .mcp.json and set the correct absolute path
```

`.mcp.json` in the project root:
```json
{
  "mcpServers": {
    "6502-simulator": {
      "command": "node",
      "args": ["mcp-server/index.js"],
      "cwd": "/absolute/path/to/6502-simulator"
    }
  }
}
```

Run `claude` from the project root — it picks up `.mcp.json` automatically.
Verify the server is loaded with `/mcp` inside the session.

**Option B — Global registration**

```bash
claude mcp add 6502-simulator node /absolute/path/to/6502-simulator/mcp-server/index.js
```

Or run the installer:
```bash
bash mcp-server/install-claude.sh
```

---

### Gemini CLI

```bash
gemini mcp add 6502-simulator node /absolute/path/to/6502-simulator/mcp-server/index.js
```

Or run the installer:
```bash
bash mcp-server/install-gemini.sh
```

Or manually edit `~/.gemini/settings.json`:
```json
{
  "mcpServers": {
    "6502-simulator": {
      "command": "node",
      "args": ["/absolute/path/to/6502-simulator/mcp-server/index.js"]
    }
  }
}
```

---

### Aider

Aider does not natively support MCP. To use the simulator alongside Aider:

- Run a Claude Code or Gemini CLI session in a separate terminal for simulator
  interaction via MCP.
- Use Aider for source file editing; switch to the MCP session to assemble and run.

---

## When the Server Is Invoked

The MCP server starts automatically when your AI tool launches. The `sim6502` process
itself does **not** start until `load_program` is called. A single simulator process
then persists for the session; a new `load_program` call restarts it with fresh code.

By default, the server looks for the `sim6502` binary in the parent directory. You can
override this by setting the `SIM6502_PATH` environment variable.

### Trigger patterns

| User intent | Tools called |
|-------------|--------------|
| Run / test assembly code | `load_program` → `run_program` → `read_registers` |
| Trace execution | `load_program` → `trace_run` |
| Step through code | `load_program` → `step_instruction` × N → `read_registers` |
| Inspect memory | `read_memory` |
| Debug a crash | `read_registers` → `disassemble` → `step_back` |
| Patch a byte | `write_memory` |
| Switch processor | `set_processor` |
| Set breakpoint and run | `set_breakpoint` → `run_program` → `read_registers` |
| Look up an opcode | `get_opcode_info` |
| TDD for a subroutine | `load_program` → `validate_routine` |
| Track memory writes | `snapshot` → `run_program` → `diff_snapshot` |
| Get a code snippet | `list_patterns` → `get_pattern` |
| VIC-II output | `vic2_info` → `vic2_savescreen` or `vic2_savebitmap` |

### Example prompts

**Basic execution**
> "Load this 6502 program and tell me the value of A after it runs:"
> ```asm
> LDA #$42
> CLC
> ADC #$01
> BRK
> ```

**Step-through debugging**
> "Step through this routine one instruction at a time and show me the registers after each step."

**Memory inspection**
> "What's at memory address $0300 after the program loads?"

**Breakpoint debugging**
> "Set a breakpoint at $020A, run the program, and show me the register state when it hits."

**Processor variants**
> "Switch to the 45gs02 processor and run this quad-load example."

**Opcode reference**
> "What addressing modes does ADC support on the 65c02?"

**Trace execution**
> "Load this routine and show me every instruction it executes with register state at each step."

**TDD / validate a routine**
> "Run these three test cases against the multiply routine at $0300 and tell me which ones pass."

**Memory diff**
> "Take a snapshot, run the program, then show me every byte that changed."

**Code snippets**
> "Show me the built-in 16-bit addition snippet and adapt it to use zero-page addresses $20 and $22."

**VIC-II / graphics**
> "Run the VIC screen demo and save a screenshot of what it renders."

**Iterative development**
> "This loop should sum the bytes at $0300–$030F into A. Run it, check the result, and fix it if it's wrong."

---

## Tool Reference

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `load_program` | Assemble and load 6502/65xx assembly code | `code` (required) |
| `run_program` | Execute until BRK, STP, or breakpoint | — |
| `step_instruction` | Execute N instructions | `count` (default 1) |
| `trace_run` | Execute N instructions; return compact per-instruction log (address, disasm, registers) | `start_address`, `max_instructions` (default 100), `stop_on_brk` |
| `step_back` | Undo one instruction | — |
| `step_forward` | Redo one instruction | — |
| `read_registers` | Get CPU state (A, X, Y, Z, B, SP, PC, Flags) | — |
| `read_memory` | Hex dump of memory | `address`, `length` (default 16) |
| `write_memory` | Write a byte to memory | `address`, `value` |
| `reset_cpu` | Reset CPU to initial state | — |
| `disassemble` | Disassemble from an address | `address`, `count` (default 15) |
| `assemble` | Inline-assemble into running memory | `code`, `address` (optional) |
| `set_breakpoint` | Set breakpoint at address | `address` |
| `clear_breakpoint` | Remove a breakpoint | `address` |
| `list_breakpoints` | List all breakpoints | — |
| `list_processors` | Show supported processor variants | — |
| `set_processor` | Change processor (6502, 65c02, 45gs02…) | `type` |
| `get_opcode_info` | Details for a specific mnemonic | `mnemonic` |
| `validate_routine` | Run register test-vectors against a subroutine; returns pass/fail per test | `routine_address`, `tests` (array), `setup` (optional asm), `scratch_address` |
| `snapshot` | Record current memory as a baseline | — |
| `diff_snapshot` | Show bytes changed since the last snapshot, with before/after and writer PC | — |
| `list_patterns` | List all built-in assembly snippets grouped by category and processor | — |
| `get_pattern` | Return the full documented source for a named snippet | `name` (e.g. `mul8_mega65`, `memcopy`) |
| `speed` | Get/set run speed throttle | `scale` (0.0=unlimited, 1.0=C64 speed) |
| `vic2_info` | VIC-II chip summary | — |
| `vic2_regs` | Full VIC-II register dump | — |
| `vic2_sprites` | VIC-II sprite states | — |
| `vic2_savescreen` | Render full screen to PPM (384×272) | `path` (optional) |
| `vic2_savebitmap` | Render display area to PPM (320×200) | `path` (optional) |

---

## Limitations

- The server cannot edit `.asm` source files on disk — code is loaded as a string via `load_program`.
- Only one simulator instance runs per session. Multiple concurrent sessions require separate server processes.
- Filesystem access is limited to a temp file (`/tmp/6502_mcp_prog.asm`) used internally.
