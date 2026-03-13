#!/bin/bash
# Register the 6502 simulator MCP server with Claude Code (claude CLI).
# Run from anywhere — uses the script's own location to find server.js.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

if ! command -v claude &>/dev/null; then
  echo "Error: 'claude' CLI not found. Install Claude Code first." >&2
  exit 1
fi

claude mcp add 6502-simulator node "$SCRIPT_DIR/index.js"
echo "Registered. Run 'claude' inside $PROJECT_DIR to use the simulator tools."
