# 6502 Simulator MCP Server

This is a Model Context Protocol (MCP) server for the 6502 simulator. It allows LLMs to interact with the simulator to load code, step through instructions, and inspect CPU/memory state.

## Features

- **Interactive Execution**: Step through 6502 assembly code.
- **State Management**: Read/Write memory and registers.
- **Symbolic Support**: Works with the simulator's core label resolution.

## Installation

1. Ensure the main simulator is built:
   ```bash
   cd ..
   make
   ```

2. Install Node.js dependencies:
   ```bash
   npm install
   ```

## Usage

Start the server using Node.js:

```bash
node server.js
```

The server communicates over `stdio` using the Model Context Protocol.

## Available Tools

- `load_program(code)`: Compiles and loads 6502 assembly code.
- `step_instruction(count)`: Executes a specific number of instructions.
- `read_registers()`: Returns current CPU state (A, X, Y, SP, PC, Flags).
- `read_memory(address, length)`: Returns a hex dump of memory.
- `write_memory(address, value)`: Writes a byte to the specified memory address.
- `reset_cpu()`: Resets the CPU state to the program start.

## Configuration for Gemini CLI

Add the following to your Gemini CLI configuration:

```json
{
  "mcpServers": {
    "6502-simulator": {
      "command": "node",
      "args": ["/path/to/6502-simulator/plugin-gemini/server.js"]
    }
  }
}
```
