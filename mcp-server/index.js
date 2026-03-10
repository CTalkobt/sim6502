#!/usr/bin/env node

import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";
import { spawn } from 'child_process';
import fs from 'fs';
import path from 'path';
import os from 'os';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const SIMULATOR_PATH = process.env.SIM6502_PATH || path.resolve(__dirname, '..', 'sim6502');

// Create a unique temporary directory for this session to avoid collisions
const sessionTmpDir = fs.mkdtempSync(path.join(os.tmpdir(), '6502-mcp-'));
const TEMP_ASM_FILE = path.join(sessionTmpDir, 'prog.asm');

// Ensure cleanup on exit
process.on('exit', () => {
  try {
    if (fs.existsSync(sessionTmpDir)) {
      fs.rmSync(sessionTmpDir, { recursive: true, force: true });
    }
  } catch (err) {
    // Silent fail on cleanup
  }
});

let simulatorProcess = null;
let simulatorBuffer  = '';
let simulatorResolve = null;

const server = new Server(
  { name: "6502-simulator", version: "0.99.0" },
  { capabilities: { tools: {} } }
);

// ── Simulator process management ──────────────────────────────────────────────

function startSimulator(asmCode) {
  if (simulatorProcess) {
    simulatorProcess.kill();
    simulatorProcess = null;
  }

  fs.writeFileSync(TEMP_ASM_FILE, asmCode);

  // -I = interactive mode, -J = JSON output for all commands
  simulatorProcess = spawn(SIMULATOR_PATH, ['-I', '-J', TEMP_ASM_FILE]);
  simulatorBuffer  = '';

  simulatorProcess.stdout.on('data', (data) => {
    simulatorBuffer += data.toString();
    if (simulatorBuffer.includes('> ') && simulatorResolve) {
      const output = simulatorBuffer.replace(/>\s*$/, '').trim();
      simulatorResolve(output);
      simulatorResolve  = null;
      simulatorBuffer   = '';
    }
  });

  simulatorProcess.stderr.on('data', (data) => {
    console.error(`Simulator: ${data}`);
  });

  simulatorProcess.on('exit', (code) => {
    if (simulatorResolve) {
      simulatorResolve(`Simulator exited with code ${code}`);
      simulatorResolve = null;
    }
    simulatorProcess = null;
  });

  return new Promise((resolve) => { simulatorResolve = resolve; });
}

function sendCommand(cmd) {
  if (!simulatorProcess)
    throw new Error("Simulator not running. Use load_program first.");

  // Sanitize: No newlines allowed in a single command, and trim whitespace
  const sanitized = cmd.replace(/[\n\r]/g, ' ').trim();
  if (sanitized.length === 0) return Promise.resolve("");

  return new Promise((resolve) => {
    simulatorResolve = resolve;
    simulatorProcess.stdin.write(sanitized + '\n');
  });
}

// ── JSON response parser ───────────────────────────────────────────────────────

/**
 * Parse a JSON envelope {"cmd":...,"ok":...,"data":...} from the simulator.
 * Returns { ok, data } on success, or throws with the error message.
 * Falls back to returning { ok: true, data: raw } if not parseable JSON.
 */
function parseResult(raw) {
  try {
    const parsed = JSON.parse(raw);
    if (parsed.ok === false) throw new Error(parsed.error || 'command failed');
    return parsed.data;
  } catch (e) {
    if (e.message && e.message !== 'command failed' && !raw.startsWith('{'))
      return raw;   // plain text fallback (e.g. initial banner)
    throw e;
  }
}

/** Format a register data object as a readable string */
function fmtRegs(d) {
  const h2 = n => n.toString(16).toUpperCase().padStart(2, '0');
  const h4 = n => n.toString(16).toUpperCase().padStart(4, '0');
  const f  = d.flags;
  return `A=$${h2(d.a)} X=$${h2(d.x)} Y=$${h2(d.y)} Z=$${h2(d.z)} B=$${h2(d.b)} ` +
         `SP=$${h2(d.sp)} PC=$${h4(d.pc)} P=$${h2(d.p)} Cycles=${d.cycles}\n` +
         `Flags: N=${f.N} V=${f.V} U=${f.U} B=${f.B} D=${f.D} I=${f.I} Z=${f.Z} C=${f.C}`;
}

/** Format an exec-stop data object (stop_reason + registers) */
function fmtStop(d) {
  return `Stopped: ${d.stop_reason} at $${d.pc.toString(16).toUpperCase().padStart(4,'0')}\n` +
         fmtRegs(d);
}

/** Format a memory data object as a hex dump */
function fmtMem(d) {
  const lines = [];
  const bytes = d.bytes;
  for (let i = 0; i < bytes.length; i += 16) {
    const addr = (d.address + i).toString(16).toUpperCase().padStart(4, '0');
    const hex  = bytes.slice(i, i + 16).map(b => b.toString(16).toUpperCase().padStart(2,'0')).join(' ');
    lines.push(`$${addr}: ${hex}`);
  }
  return lines.join('\n');
}

/** Format a disasm instructions array as a listing */
function fmtDisasm(d) {
  return d.instructions.map(i => {
    const addr = i.address.toString(16).toUpperCase().padStart(4, '0');
    const bytes = i.bytes.padEnd(12);
    const mnem  = i.mnemonic.padEnd(6);
    return `$${addr}  ${bytes}  ${mnem} ${i.operand}`;
  }).join('\n');
}

/** Format an info/opcode data object */
function fmtInfo(d) {
  const lines = [`${d.mnemonic}:`, '  Mode                 Syntax            Cyc(6502/65c02/65ce02/45gs02)  Opcode  Size'];
  for (const m of d.modes) {
    lines.push(`  ${m.mode.padEnd(22)}${m.syntax.padEnd(18)}` +
               `${m.cycles_6502}/${m.cycles_65c02}/${m.cycles_65ce02}/${m.cycles_45gs02}`.padEnd(29) +
               `  ${m.opcode.padEnd(8)}${m.size}`);
  }
  return lines.join('\n');
}

/** Format a trace_run instruction array as a compact log */
function fmtTrace(d) {
  const h4 = n => n.toString(16).toUpperCase().padStart(4, '0');
  const h2 = n => n.toString(16).toUpperCase().padStart(2, '0');
  const lines = d.instructions.map(i => {
    if (i.stop) {
      return `$${h4(i.pc)}  ${i.mnemonic}${i.operand ? ' ' + i.operand : ''}`.padEnd(30) + `  (stop)`;
    }
    const instr = (i.mnemonic + (i.operand ? ' ' + i.operand : '')).padEnd(20);
    return `$${h4(i.pc)}  ${instr}  A=${h2(i.a)} X=${h2(i.x)} Y=${h2(i.y)} P=${h2(i.p)} SP=${h2(i.sp)}  cycles=${i.cycles}`;
  });
  lines.push(`---`);
  lines.push(`Stopped: ${d.stop_reason}  Executed: ${d.count}  Cycles: ${d.total_cycles}`);
  return lines.join('\n');
}

/** Format a breakpoint list */
function fmtBreakpoints(d) {
  if (d.breakpoints.length === 0) return 'No breakpoints set.';
  return d.breakpoints.map(b =>
    `  ${b.index}: $${b.address.toString(16).toUpperCase().padStart(4,'0')} ` +
    `[${b.enabled ? 'enabled' : 'disabled'}]` +
    (b.condition ? ` if ${b.condition}` : '')
  ).join('\n');
}

// ── Tool Registry ─────────────────────────────────────────────────────────────

server.setRequestHandler(ListToolsRequestSchema, async () => ({
  tools: [
    {
      name: "load_program",
      description: "Assemble and load 6502/65xx assembly code into the simulator.",
      inputSchema: { type: "object", properties: {
        code: { type: "string", description: "Assembly source code" }
      }, required: ["code"] }
    },
    {
      name: "run_program",
      description: "Run until BRK, STP, or a breakpoint. Returns stop reason and final registers.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "step_instruction",
      description: "Execute N instructions. Returns stop reason and registers after stepping.",
      inputSchema: { type: "object", properties: {
        count: { type: "number", description: "Number of instructions (default 1)" }
      }}
    },
    {
      name: "step_back",
      description: "Undo one instruction (restore pre-execute CPU and memory state).",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "step_forward",
      description: "Re-execute one instruction from history. Only valid after step_back.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "read_registers",
      description: "Read all CPU registers and flags.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "read_memory",
      description: "Read a range of memory bytes.",
      inputSchema: { type: "object", properties: {
        address: { type: "number", description: "Start address" },
        length:  { type: "number", description: "Byte count (default 16)" }
      }, required: ["address"] }
    },
    {
      name: "write_memory",
      description: "Write a single byte to memory.",
      inputSchema: { type: "object", properties: {
        address: { type: "number", description: "Target address" },
        value:   { type: "number", description: "Byte value (0–255)" }
      }, required: ["address", "value"] }
    },
    {
      name: "disassemble",
      description: "Disassemble instructions from memory. Returns address, bytes, mnemonic, operand, and cycle count for each instruction.",
      inputSchema: { type: "object", properties: {
        address: { type: "number", description: "Start address (default: current PC)" },
        count:   { type: "number", description: "Number of instructions (default 15)" }
      }}
    },
    {
      name: "assemble",
      description: "Inline-assemble code into running memory. Accepts pseudo-ops (.org, .byte, .word, .text, .align).",
      inputSchema: { type: "object", properties: {
        code:    { type: "string", description: "Assembly source (one instruction per line)" },
        address: { type: "number", description: "Start address (default: current PC)" }
      }, required: ["code"] }
    },
    {
      name: "reset_cpu",
      description: "Reset the CPU to its initial state.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "set_breakpoint",
      description: "Set a breakpoint at an address.",
      inputSchema: { type: "object", properties: {
        address: { type: "number", description: "Breakpoint address" }
      }, required: ["address"] }
    },
    {
      name: "clear_breakpoint",
      description: "Remove a breakpoint.",
      inputSchema: { type: "object", properties: {
        address: { type: "number", description: "Breakpoint address to remove" }
      }, required: ["address"] }
    },
    {
      name: "list_breakpoints",
      description: "List all set breakpoints.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "list_processors",
      description: "List supported processor variants.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "set_processor",
      description: "Switch the active processor (6502, 65c02, 65ce02, 45gs02).",
      inputSchema: { type: "object", properties: {
        type: { type: "string", description: "Processor name" }
      }, required: ["type"] }
    },
    {
      name: "get_opcode_info",
      description: "Get addressing modes, cycle counts, opcodes, and syntax for a mnemonic across all supported processors.",
      inputSchema: { type: "object", properties: {
        mnemonic: { type: "string", description: "Instruction mnemonic (e.g. LDA, ADC)" }
      }, required: ["mnemonic"] }
    },
    {
      name: "speed",
      description: "Get or set run speed. scale=1.0 = C64 PAL speed, 0.0 = unlimited.",
      inputSchema: { type: "object", properties: {
        scale: { type: "number", description: "Speed multiplier (omit to query)" }
      }}
    },
    {
      name: "trace_run",
      description: "Execute N instructions and return a compact per-instruction log showing address, disassembly, and register state after each step. Efficient alternative to calling step_instruction + read_registers in a loop.",
      inputSchema: { type: "object", properties: {
        start_address:    { type: "number", description: "Start address (default: current PC)" },
        max_instructions: { type: "number", description: "Maximum instructions to execute (default 100, max 2000)" },
        stop_on_brk:      { type: "boolean", description: "Stop when BRK is reached (default true)" }
      }}
    },
    {
      name: "validate_routine",
      description: "Run an array of register test-vectors against a subroutine. For each test: sets input registers, calls the routine via JSR, captures output registers, and checks expected values. Returns pass/fail per test. Dramatically faster than step_instruction + read_registers loops for TDD.",
      inputSchema: { type: "object", required: ["routine_address", "tests"], properties: {
        routine_address: { type: "number", description: "Address of the subroutine (e.g. 768 = $0300)" },
        scratch_address: { type: "number", description: "4-byte scratch area for the JSR+BRK shim (default $FFF8)" },
        setup:           { type: "string", description: "Optional assembly snippet executed ONCE before all tests to initialise memory (e.g. 'LDA #$00\\nSTA $FB')" },
        tests: {
          type: "array",
          description: "Array of test vectors",
          items: {
            type: "object",
            properties: {
              label:  { type: "string" },
              in:     { type: "object", description: "Input registers: {a,x,y,z,b,s,p} (omit to leave unset)" },
              expect: { type: "object", description: "Expected output registers: {a,x,y,z,b,s,p} (omit to skip check)" }
            }
          }
        }
      }}
    },
    {
      name: "list_patterns",
      description: "List all built-in assembly snippet templates (idioms). Use get_pattern to retrieve the full documented source for a specific snippet.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "get_pattern",
      description: "Return a fully documented, parameterised assembly snippet ready to inline or adapt. Use list_patterns to see available names.",
      inputSchema: {
        type: "object",
        required: ["name"],
        properties: {
          name: { type: "string", description: "Snippet name, e.g. 'mul8_mega65', 'memcopy'. Use list_patterns to see all names." }
        }
      }
    },
    {
      name: "snapshot",
      description: "Capture current memory state. Subsequent run/step/trace_run calls track every write. Use diff_snapshot to see what changed.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "diff_snapshot",
      description: "Compare current memory to the last snapshot. Returns every address whose value differs, with before/after values and the PC of the instruction that last wrote it. Consecutive changed addresses are shown as ranges.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "vic2_info",
      description: "VIC-II state summary: mode, key addresses, colours.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "vic2_regs",
      description: "Full VIC-II register dump with all decoded fields.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "vic2_sprites",
      description: "All 8 VIC-II sprite states.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "vic2_savescreen",
      description: "Render full VIC-II frame (384×272) to a PPM file.",
      inputSchema: { type: "object", properties: {
        path: { type: "string", description: "Output file path (default: /tmp/vic2screen.ppm)" }
      }}
    },
    {
      name: "vic2_savebitmap",
      description: "Render 320×200 active display area to a PPM file.",
      inputSchema: { type: "object", properties: {
        path: { type: "string", description: "Output file path (default: /tmp/vic2bitmap.ppm)" }
      }}
    },
    {
      name: "list_env_templates",
      description: "List all available project environment templates.",
      inputSchema: { type: "object", properties: {} }
    },
    {
      name: "create_project",
      description: "Create a new project directory structure from a template.",
      inputSchema: { 
        type: "object", 
        required: ["template_id", "project_name"],
        properties: {
          template_id:  { type: "string", description: "The ID of the template to use (e.g. 'mega65-basic')" },
          project_name: { type: "string", description: "The name of the project" },
          target_dir:   { type: "string", description: "Target directory path (optional, defaults to project_name)" },
          variables:    { type: "object", description: "Optional variable overrides (e.g. { 'CPU': '6502' })" }
        }
      }
    },
    {
      name: "shutdown_simulator",
      description: "Terminate the persistent simulator process and free system resources.",
      inputSchema: { type: "object", properties: {} }
    }
  ]
}));

// ── Tool Handlers ─────────────────────────────────────────────────────────────

server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: args } = request.params;

  const text = (t) => ({ content: [{ type: "text", text: String(t) }] });
  const err  = (e) => ({ content: [{ type: "text", text: `Error: ${e}` }], isError: true });

  try {
    switch (name) {

      case "load_program": {
        await startSimulator(args.code);
        const symRaw = await sendCommand("symbols");
        const symData = parseResult(symRaw);
        const h4 = n => n.toString(16).toUpperCase().padStart(4, '0');
        const lines = ["Program loaded successfully."];
        if (symData.count > 0) {
          lines.push(`\nResolved symbols (${symData.count}):`);
          for (const s of symData.symbols) {
            lines.push(`  ${s.name.padEnd(20)} $${h4(s.address)}  (${s.type})`);
          }
        } else {
          lines.push("No symbols defined.");
        }
        return text(lines.join('\n'));
      }

      case "run_program": {
        const raw = await sendCommand("run");
        return text(fmtStop(parseResult(raw)));
      }

      case "step_instruction": {
        const count = args.count || 1;
        const raw = await sendCommand(`step ${count}`);
        return text(fmtStop(parseResult(raw)));
      }

      case "step_back": {
        const raw = await sendCommand("sb");
        const d = parseResult(raw);
        return text(fmtStop(d));
      }

      case "step_forward": {
        const raw = await sendCommand("sf");
        const d = parseResult(raw);
        return text(fmtStop(d));
      }

      case "read_registers": {
        const raw = await sendCommand("regs");
        return text(fmtRegs(parseResult(raw)));
      }

      case "read_memory": {
        const addr = args.address;
        const len  = args.length || 16;
        const raw  = await sendCommand(`mem $${addr.toString(16)} ${len}`);
        return text(fmtMem(parseResult(raw)));
      }

      case "write_memory": {
        const addr = `$${args.address.toString(16)}`;
        const val  = `$${args.value.toString(16)}`;
        const raw  = await sendCommand(`write ${addr} ${val}`);
        parseResult(raw); // throws on error
        return text(`Written $${args.value.toString(16).toUpperCase().padStart(2,'0')} to $${args.address.toString(16).toUpperCase().padStart(4,'0')}`);
      }

      case "disassemble": {
        const count    = args.count || 15;
        const addrPart = args.address !== undefined ? ` $${args.address.toString(16)}` : '';
        const raw = await sendCommand(`disasm${addrPart} ${count}`);
        return text(fmtDisasm(parseResult(raw)));
      }

      case "assemble": {
        const addrPart = args.address !== undefined ? `$${args.address.toString(16)} ` : '';
        const lines = (args.code || '').split('\n').filter(l => l.trim());
        const results = [];
        let hasErrors = false;
        
        for (let i = 0; i < lines.length; i++) {
          const raw = await sendCommand(`asm ${addrPart}${lines[i]}`);
          try {
            const d = parseResult(raw);
            results.push(`$${d.address.toString(16).toUpperCase().padStart(4,'0')}: ${d.bytes.padEnd(8)}  ${lines[i]}`);
          } catch (e) {
            hasErrors = true;
            results.push(`Line ${i + 1}: ${lines[i]}\n  Error: ${e.message}`);
          }
        }
        const out = results.join('\n');
        return text(hasErrors ? `Assembly completed with errors:\n${out}` : out);
      }

      case "validate_routine": {
        const h2 = n => (n & 0xFF).toString(16).padStart(2,'0').toUpperCase();
        const h4 = n => (n & 0xFFFF).toString(16).padStart(4,'0').toUpperCase();
        const routineAddr = args.routine_address;
        const scratchAddr = args.scratch_address || 0xFFF8;
        const tests       = args.tests || [];

        // Handle optional setup: assemble code at the routine address (or a safe place) and run it
        if (args.setup) {
          const setupLines = args.setup.split('\n').filter(l => l.trim());
          // Assemble setup code at a temporary scratch location to avoid overwriting the routine
          const setupBase = (scratchAddr - setupLines.length * 3) & 0xFFF0; 
          let currentAsm = setupBase;
          for (const line of setupLines) {
            const raw = await sendCommand(`asm $${h4(currentAsm)} ${line}`);
            const d = parseResult(raw);
            currentAsm += d.size;
          }
          // End with BRK
          await sendCommand(`asm $${h4(currentAsm)} BRK`);
          // Run setup
          await sendCommand(`trace $${h4(setupBase)} 10000 1`);
        }

        const results = [];
        for (let i = 0; i < tests.length; i++) {
          const test = tests[i];
          const inp  = test.in     || {};
          const exp  = test.expect || {};

          let vcmd = `validate $${h4(routineAddr)}`;
          if (scratchAddr !== undefined) vcmd += ` scratch=$${h4(scratchAddr)}`;
          for (const [k, v] of Object.entries(inp)) vcmd += ` ${k.toUpperCase()}=${v}`;
          vcmd += ' :';
          for (const [k, v] of Object.entries(exp)) vcmd += ` ${k.toUpperCase()}=${v}`;

          const raw = await sendCommand(vcmd);
          const d   = parseResult(raw);
          results.push({
            label:    test.label || `test ${i + 1}`,
            passed:   d.passed,
            actual:   d.actual,
            fail_msg: d.fail_msg || '',
          });
        }

        const passCount = results.filter(r => r.passed).length;
        const lines = [`${passCount}/${results.length} test(s) passed\n`];
        for (const r of results) {
          const icon = r.passed ? '✓' : '✗';
          lines.push(`  ${icon} ${r.label}`);
          if (!r.passed) {
            if (r.fail_msg) lines.push(`       ${r.fail_msg.trim()}`);
            if (r.actual) {
              const a = r.actual;
              lines.push(`       actual: A=$${h2(a.a)} X=$${h2(a.x)} Y=$${h2(a.y)} Z=$${h2(a.z)} B=$${h2(a.b)} P=$${h2(a.p)}`);
            }
          }
        }
        return text(lines.join('\n'));
      }

      case "list_patterns": {
        const raw  = await sendCommand("list_patterns");
        const d    = parseResult(raw);
        const pats = d.patterns;
        const byCat = {};
        for (const p of pats) {
          if (!byCat[p.category]) byCat[p.category] = [];
          byCat[p.category].push(p);
        }
        const lines = [`Available snippets (${pats.length}):\n`];
        for (const [cat, items] of Object.entries(byCat)) {
          lines.push(`[${cat}]`);
          for (const p of items)
            lines.push(`  ${p.name.padEnd(24)} ${p.processor.padEnd(8)} ${p.summary}`);
          lines.push('');
        }
        lines.push('Use get_pattern <name> for the full documented source.');
        return text(lines.join('\n'));
      }

      case "get_pattern": {
        const name = args.name || '';
        const raw  = await sendCommand(`get_pattern ${name}`);
        const d    = parseResult(raw);
        return text(`; ${d.name}  [${d.category} / ${d.processor}]\n; ${d.summary}\n\n${d.body}`);
      }

      case "snapshot": {
        await sendCommand("snapshot");
        return text("Memory snapshot taken. Run/step your code, then call diff_snapshot.");
      }

      case "diff_snapshot": {
        const raw = await sendCommand("diff");
        const d   = parseResult(raw);
        if (d.count === 0) return text("No memory changes since snapshot.");
        const h4 = n => n.toString(16).toUpperCase().padStart(4, '0');
        const lines = [`Memory diff: ${d.count} change(s)\n`];
        // Collapse consecutive addresses into ranges
        const changes = d.changes;
        let i = 0;
        while (i < changes.length) {
          let j = i;
          while (j + 1 < changes.length && changes[j+1].addr === changes[j].addr + 1) j++;
          if (i === j) {
            const c = changes[i];
            lines.push(`  $${h4(c.addr)}        ${h4(c.before).slice(-2)}->${h4(c.after).slice(-2)}  by $${h4(c.writer_pc)}`);
          } else {
            const span = j - i + 1;
            const preview = changes.slice(i, Math.min(i+8, j+1));
            const bef = preview.map(c => c.before.toString(16).padStart(2,'0')).join(' ');
            const aft = preview.map(c => c.after .toString(16).padStart(2,'0')).join(' ');
            const ellipsis = span > 8 ? ' ...' : '';
            lines.push(`  $${h4(changes[i].addr)}-$${h4(changes[j].addr)}  ${span} byte(s): [${bef}${ellipsis}]->[${aft}${ellipsis}]  by $${h4(changes[j].writer_pc)}`);
          }
          i = j + 1;
        }
        return text(lines.join('\n'));
      }

    case "trace_run": {
        const maxN  = Math.min(args.max_instructions || 100, 2000);
        const brk   = args.stop_on_brk !== false ? 1 : 0;
        const cmd   = args.start_address !== undefined
          ? `trace $${args.start_address.toString(16)} ${maxN} ${brk}`
          : `trace ${maxN} ${brk}`;
        const raw = await sendCommand(cmd);
        return text(fmtTrace(parseResult(raw)));
      }

      case "shutdown_simulator": {
        if (simulatorProcess) {
          simulatorProcess.kill();
          simulatorProcess = null;
          return text("Simulator process terminated.");
        }
        return text("Simulator was not running.");
      }

      case "reset_cpu": {
        const raw = await sendCommand("reset");
        parseResult(raw);
        return text("CPU reset.");
      }

      case "set_breakpoint": {
        const addr = `$${args.address.toString(16)}`;
        const raw  = await sendCommand(`break ${addr}`);
        parseResult(raw);
        return text(`Breakpoint set at $${args.address.toString(16).toUpperCase().padStart(4,'0')}`);
      }

      case "clear_breakpoint": {
        const addr = `$${args.address.toString(16)}`;
        const raw  = await sendCommand(`clear ${addr}`);
        parseResult(raw);
        return text(`Breakpoint cleared at $${args.address.toString(16).toUpperCase().padStart(4,'0')}`);
      }

      case "list_breakpoints": {
        const raw = await sendCommand("list");
        return text(fmtBreakpoints(parseResult(raw)));
      }

      case "list_processors": {
        const raw = await sendCommand("processors");
        const d   = parseResult(raw);
        return text(`Supported processors: ${d.processors.join(', ')}`);
      }

      case "set_processor": {
        const raw = await sendCommand(`processor ${args.type}`);
        parseResult(raw);
        return text(`Processor set to ${args.type}`);
      }

      case "get_opcode_info": {
        const raw = await sendCommand(`info ${args.mnemonic}`);
        return text(fmtInfo(parseResult(raw)));
      }

      case "speed": {
        const cmd = args.scale !== undefined ? `speed ${args.scale}` : "speed";
        const raw = await sendCommand(cmd);
        const d   = parseResult(raw);
        return text(d.unlimited ? "Speed: unlimited" : `Speed: ${d.scale}× C64 (${d.hz.toFixed(0)} Hz)`);
      }

      case "vic2_info": {
        const raw = await sendCommand("vic2.info");
        return text(JSON.stringify(parseResult(raw), null, 2));
      }

      case "vic2_regs": {
        const raw = await sendCommand("vic2.regs");
        return text(JSON.stringify(parseResult(raw), null, 2));
      }

      case "vic2_sprites": {
        const raw = await sendCommand("vic2.sprites");
        const d   = parseResult(raw);
        const lines = [`MC0: ${d.mc0} (${d.mc0_name})  MC1: ${d.mc1} (${d.mc1_name})`];
        for (const s of d.sprites) {
          lines.push(`  #${s.index} ${s.enabled ? 'on ' : 'off'} ` +
            `X=${String(s.x).padStart(3)} Y=${String(s.y).padStart(3)} ` +
            `col=${s.color}(${s.color_name}) ` +
            `mcm=${s.multicolor} xexp=${s.expand_x} yexp=${s.expand_y} ` +
            `bg=${s.behind_bg} data=$${s.data_addr.toString(16).toUpperCase().padStart(4,'0')}`);
        }
        return text(lines.join('\n'));
      }

      case "vic2_savescreen": {
        const filePath = (args.path && args.path.trim()) || path.join(os.tmpdir(), 'vic2screen.ppm');
        const raw = await sendCommand(`vic2.savescreen ${filePath}`);
        const d   = parseResult(raw);
        return text(`Saved ${d.width}×${d.height} PPM to '${d.path}'`);
      }

      case "vic2_savebitmap": {
        const filePath = (args.path && args.path.trim()) || path.join(os.tmpdir(), 'vic2bitmap.ppm');
        const raw = await sendCommand(`vic2.savebitmap ${filePath}`);
        const d   = parseResult(raw);
        return text(`Saved ${d.width}×${d.height} active-area PPM to '${d.path}'`);
      }

      case "list_env_templates": {
        const raw = await sendCommand("env list");
        const d   = parseResult(raw);
        const lines = ["Available Project Templates:\n"];
        for (const t of d.templates) {
          lines.push(`  ID: ${t.id.padEnd(16)} Name: ${t.name}`);
          lines.push(`      ${t.description}`);
        }
        return text(lines.join('\n'));
      }

      case "create_project": {
        const tid  = args.template_id;
        const name = args.project_name;
        const dir  = args.target_dir || name;
        const vars = args.variables || {};
        
        let cmd = `env create ${tid} "${name}" "${dir}"`;
        for (const [k, v] of Object.entries(vars)) {
          cmd += ` ${k}=${v}`;
        }
        
        const raw = await sendCommand(cmd);
        const d   = parseResult(raw);
        return text(`Project '${name}' created successfully in: ${d.path}`);
      }

      default:
        throw new Error(`Unknown tool: ${name}`);
    }
  } catch (e) {
    return err(e.message);
  }
});

// ── Start ─────────────────────────────────────────────────────────────────────

const transport = new StdioServerTransport();
console.error("6502-simulator MCP server starting...");
await server.connect(transport);
console.error("6502-simulator MCP server running on stdio");
