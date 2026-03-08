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

const SIMULATOR_PATH = path.resolve(__dirname, '..', 'sim6502');
const TEMP_ASM_FILE  = path.join(os.tmpdir(), '6502_mcp_prog.asm');

let simulatorProcess = null;
let simulatorBuffer  = '';
let simulatorResolve = null;

const server = new Server(
  { name: "6502-simulator", version: "1.0.0" },
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
  return new Promise((resolve) => {
    simulatorResolve = resolve;
    simulatorProcess.stdin.write(cmd + '\n');
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
        return text("Program loaded successfully.");
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
        // Assembly mode uses interactive prompts — runs in text mode regardless of -J
        const addrPart = args.address !== undefined ? ` $${args.address.toString(16)}` : '';
        const lines = (args.code || '').split('\n').filter(l => l.trim());
        const parts = [];
        parts.push(await sendCommand(`asm${addrPart}`));
        let hasErrors = false;
        for (let i = 0; i < lines.length; i++) {
          const response = await sendCommand(lines[i]);
          if (response && response.includes('error:')) {
            hasErrors = true;
            // Prefix with line number and original source so the error is unambiguous
            parts.push(`Line ${i + 1}: ${lines[i]}\n${response}`);
          } else {
            parts.push(response);
          }
        }
        parts.push(await sendCommand('.'));
        const out = parts.filter(Boolean).join('\n');
        return text(hasErrors ? `Assembly completed with errors:\n${out}` : out);
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
        const filePath = (args.path && args.path.trim()) || '/tmp/vic2screen.ppm';
        const raw = await sendCommand(`vic2.savescreen ${filePath}`);
        const d   = parseResult(raw);
        return text(`Saved ${d.width}×${d.height} PPM to '${d.path}'`);
      }

      case "vic2_savebitmap": {
        const filePath = (args.path && args.path.trim()) || '/tmp/vic2bitmap.ppm';
        const raw = await sendCommand(`vic2.savebitmap ${filePath}`);
        const d   = parseResult(raw);
        return text(`Saved ${d.width}×${d.height} active-area PPM to '${d.path}'`);
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
await server.connect(transport);
