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

// Determine simulator path
const SIMULATOR_PATH = path.resolve(__dirname, '..', 'sim6502');

// Temp file for ASM code
const TEMP_ASM_FILE = path.join(os.tmpdir(), 'gemini_6502_prog.asm');

// Simulator process handle
let simulatorProcess = null;
let simulatorBuffer = '';
let simulatorResolve = null;

// Initialize MCP Server
const server = new Server(
  {
    name: "gemini-6502-simulator",
    version: "1.0.0",
  },
  {
    capabilities: {
      tools: {},
    },
  }
);

// Helper: Start Simulator
function startSimulator(asmCode) {
  if (simulatorProcess) {
    simulatorProcess.kill();
    simulatorProcess = null;
  }

  fs.writeFileSync(TEMP_ASM_FILE, asmCode);

  simulatorProcess = spawn(SIMULATOR_PATH, ['-I', TEMP_ASM_FILE]);
  simulatorBuffer = '';

  simulatorProcess.stdout.on('data', (data) => {
    const chunk = data.toString();
    simulatorBuffer += chunk;
    
    // Check for prompt
    if (simulatorBuffer.includes('> ')) {
      if (simulatorResolve) {
        const output = simulatorBuffer.replace(/> $/, '').trim();
        simulatorResolve(output);
        simulatorResolve = null;
        simulatorBuffer = '';
      }
    }
  });

  simulatorProcess.stderr.on('data', (data) => {
    console.error(`Simulator Error: ${data}`);
  });

  simulatorProcess.on('exit', (code) => {
    if (simulatorResolve) {
      simulatorResolve(`Simulator exited with code ${code}`);
      simulatorResolve = null;
    }
    simulatorProcess = null;
  });

  // Wait for initial prompt
  return new Promise((resolve) => {
    simulatorResolve = resolve;
  });
}

// Helper: Send Command
function sendCommand(cmd) {
  if (!simulatorProcess) {
    throw new Error("Simulator not running. Load a program first.");
  }
  
  return new Promise((resolve) => {
    simulatorResolve = resolve;
    simulatorProcess.stdin.write(cmd + '
');
  });
}

// Register Tools
server.setRequestHandler(ListToolsRequestSchema, async () => {
  return {
    tools: [
      {
        name: "load_program",
        description: "Compiles and loads 6502 assembly code into the simulator.",
        inputSchema: {
          type: "object",
          properties: {
            code: {
              type: "string",
              description: "The assembly code to load (e.g., LDA #$00...)",
            },
          },
          required: ["code"],
        },
      },
      {
        name: "step_instruction",
        description: "Executes N instructions.",
        inputSchema: {
          type: "object",
          properties: {
            count: {
              type: "number",
              description: "Number of instructions to execute (default 1)",
            },
          },
        },
      },
      {
        name: "read_registers",
        description: "Reads current CPU registers (A, X, Y, SP, PC, Flags).",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "read_memory",
        description: "Reads a range of memory.",
        inputSchema: {
          type: "object",
          properties: {
            address: {
              type: "number",
              description: "Start address (decimal or hex)",
            },
            length: {
              type: "number",
              description: "Number of bytes to read (default 16)",
            },
          },
          required: ["address"],
        },
      },
      {
        name: "write_memory",
        description: "Writes a byte to memory.",
        inputSchema: {
          type: "object",
          properties: {
            address: {
              type: "number",
              description: "Address to write to",
            },
            value: {
              type: "number",
              description: "Value to write (0-255)",
            },
          },
          required: ["address", "value"],
        },
      },
      {
        name: "reset_cpu",
        description: "Resets the CPU to initial state.",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "list_processors",
        description: "Lists all supported processor types.",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "set_processor",
        description: "Sets the processor architecture.",
        inputSchema: {
          type: "object",
          properties: {
            type: {
              type: "string",
              description: "Processor type (e.g., '6502', '65c02', '45gs02')",
            },
          },
          required: ["type"],
        },
      },
      {
        name: "get_opcode_info",
        description: "Gets information about a specific opcode.",
        inputSchema: {
          type: "object",
          properties: {
            mnemonic: {
              type: "string",
              description: "Opcode mnemonic (e.g., 'LDA', 'RTS')",
            },
          },
          required: ["mnemonic"],
        },
      },
    ],
  };
});

// Handle Tool Calls
server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: args } = request.params;

  try {
    if (name === "load_program") {
      await startSimulator(args.code);
      return {
        content: [{ type: "text", text: "Program loaded successfully." }],
      };
    } else if (name === "step_instruction") {
      const count = args.count || 1;
      const output = await sendCommand(`step ${count}`);
      return {
        content: [{ type: "text", text: output }],
      };
    } else if (name === "read_registers") {
      const output = await sendCommand("regs");
      return {
        content: [{ type: "text", text: output }],
      };
    } else if (name === "read_memory") {
      const addr = args.address; // Handle hex/dec
      const len = args.length || 16;
      // Convert decimal addr to hex string for simulator if needed
      // Simulator accepts hex input for address
      const hexAddr = addr.toString(16);
      const output = await sendCommand(`mem ${hexAddr} ${len}`);
      return {
        content: [{ type: "text", text: output }],
      };
    } else if (name === "write_memory") {
      const addr = args.address.toString(16);
      const val = args.value.toString(16);
      const output = await sendCommand(`write ${addr} ${val}`);
      return {
        content: [{ type: "text", text: output }],
      };
    } else if (name === "reset_cpu") {
      const output = await sendCommand("reset");
      return {
        content: [{ type: "text", text: output }],
      };
    } else if (name === "list_processors") {
      const output = await sendCommand("processors");
      return {
        content: [{ type: "text", text: output }],
      };
    } else if (name === "set_processor") {
      const output = await sendCommand(`processor ${args.type}`);
      return {
        content: [{ type: "text", text: output }],
      };
    } else if (name === "get_opcode_info") {
      const output = await sendCommand(`info ${args.mnemonic}`);
      return {
        content: [{ type: "text", text: output }],
      };
    } else {
      throw new Error(`Unknown tool: ${name}`);
    }
  } catch (error) {
    return {
      content: [{ type: "text", text: `Error: ${error.message}` }],
      isError: true,
    };
  }
});

// Start Server
const transport = new StdioServerTransport();
await server.connect(transport);
