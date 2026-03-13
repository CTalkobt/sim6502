#!/bin/bash
echo "Installing 6502 Simulator Plugin for Gemini CLI..."
GEMINI_PLUGIN_DIR="$HOME/.gemini/plugins"
PLUGIN_NAME="6502-simulator"
PLUGIN_DIR="$GEMINI_PLUGIN_DIR/$PLUGIN_NAME"

if [ ! -d "$GEMINI_PLUGIN_DIR" ]; then
    mkdir -p "$GEMINI_PLUGIN_DIR"
fi

if [ -d "$PLUGIN_DIR" ]; then
    echo "Plugin already installed. Overwriting..."
    rm -rf "$PLUGIN_DIR"
fi

mkdir -p "$PLUGIN_DIR"

cp -r ./* "$PLUGIN_DIR"

echo "Installation complete."
echo "To use the MCP server, add the following to your gemini config file (~/.gemini/settings.json):"
echo ""
echo "  \"mcpServers\": {"
echo "    \"6502-simulator\": {"
echo "      \"command\": \"node\","
echo "      \"args\": [\"$PLUGIN_DIR/index.js\"]"
echo "    }"
echo "  }"
echo ""