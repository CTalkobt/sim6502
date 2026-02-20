# 6502 Simulator Claude Plugin - Installation Guide

## Overview

This package contains the complete Claude plugin for the 6502 Simulator.

## Contents

```
6502-plugin.zip
├── plugin/
│   ├── index.js                    # Main plugin file
│   ├── package.json                # Package metadata
│   ├── config.json.template        # Configuration template
│   ├── README.md                   # Plugin documentation
│   ├── LICENSE                     # License file
│   ├── install.sh                  # Linux/macOS installer
│   └── install.bat                 # Windows installer
├── INSTALLATION_GUIDE.md           # This file
└── QUICK_START.txt                 # Quick start instructions
```

## Prerequisites

Before installing the plugin, you need:

1. **6502 Simulator** - Built from source
   - Download and build the simulator first
   - Make sure `sim6502` executable exists
   - Verify symbol tables are in `symbols/` directory

2. **Claude** - Latest version
   - Desktop or mobile version
   - Node.js compatible environment (for plugin)

3. **Basic Tools**
   - Unzip utility (to extract plugin.zip)
   - Text editor (to edit config.json)

## Quick Installation (5 minutes)

### Step 1: Extract Files

```bash
unzip 6502-plugin.zip
cd plugin
```

### Step 2: Run Installer

#### Linux/macOS:
```bash
chmod +x install.sh
./install.sh /path/to/6502-simulator
```

#### Windows:
```cmd
install.bat C:\path\to\6502-simulator
```

#### Manual (All Platforms):
```bash
mkdir -p ~/.claude/plugins/6502-simulator
cp plugin/* ~/.claude/plugins/6502-simulator/
cp config.json.template ~/.claude/plugins/6502-simulator/config.json
# Edit config.json with your paths
```

### Step 3: Restart Claude

1. Close all Claude windows
2. Reopen Claude
3. Type `/help` to verify

## Manual Installation

If the installer doesn't work for you:

1. **Create directory:**
   ```bash
   mkdir -p ~/.claude/plugins/6502-simulator
   ```

2. **Copy files:**
   ```bash
   cp plugin/index.js ~/.claude/plugins/6502-simulator/
   cp plugin/package.json ~/.claude/plugins/6502-simulator/
   cp plugin/README.md ~/.claude/plugins/6502-simulator/
   ```

3. **Create config.json:**
   ```bash
   cp plugin/config.json.template ~/.claude/plugins/6502-simulator/config.json
   ```

4. **Edit config.json:**
   - Open with text editor
   - Replace `/path/to/6502-simulator` with actual path
   - Save file

5. **Restart Claude**

## Configuration

### Finding Your Paths

**Simulator Path:**
```bash
which sim6502
# or
cd 6502-simulator && pwd && ls sim6502
```

**Symbol Directory:**
```bash
ls 6502-simulator/symbols/
```

### config.json Structure

```json
{
  "simulator_path": "/full/path/to/sim6502",
  "work_directory": "/tmp/6502-work",
  "symbol_paths": {
    "c64": "/full/path/to/symbols/c64.sym",
    "c128": "/full/path/to/symbols/c128.sym",
    "mega65": "/full/path/to/symbols/mega65.sym",
    "x16": "/full/path/to/symbols/x16.sym"
  },
  "max_execution_time": 30,
  "auto_cleanup": true
}
```

**Important:** Use absolute paths (starting with `/` or `C:\`)

## Verification

### Test Installation

In Claude, type:
```
/help
```

Expected output: Plugin help message

### Test Execution

```
/asm -p 6502

LDA #$42
STA $1000
```

Expected output: Simulator execution results

### Test Symbol Tables

```
/symbols
```

Expected output: List of available symbol tables

## Troubleshooting

### Plugin Not Appearing

1. Verify directory exists:
   ```bash
   ls ~/.claude/plugins/6502-simulator/
   ```

2. Check files:
   - config.json (should exist)
   - index.js (should exist)
   - package.json (should exist)

3. Verify JSON syntax:
   ```bash
   cat config.json | jq .
   ```

4. Restart Claude completely

### "Simulator Not Found"

1. Check simulator built:
   ```bash
   /path/to/sim6502 --help
   ```

2. Update config.json with correct path

3. Verify executable permissions:
   ```bash
   chmod +x /path/to/sim6502
   ```

### Symbol Tables Missing

1. Verify files exist:
   ```bash
   ls /path/to/symbols/
   ```

2. Check file permissions:
   ```bash
   chmod 644 /path/to/symbols/*.sym
   ```

3. Update config.json paths

4. Test: `/symbols` command

## Platform-Specific Notes

### Linux

```bash
# Typical installation
./install.sh ~/6502-simulator

# Or manual
mkdir -p ~/.claude/plugins/6502-simulator
cp plugin/* ~/.claude/plugins/6502-simulator/
```

### macOS

```bash
# Similar to Linux
./install.sh ~/6502-simulator

# Or manual - paths are same as Linux
```

### Windows

```cmd
# Using installer
install.bat C:\Users\YourName\6502-simulator

# Or manual
mkdir %USERPROFILE%\.claude\plugins\6502-simulator
copy plugin\* %USERPROFILE%\.claude\plugins\6502-simulator\
```

## Next Steps

1. **Read Documentation:**
   - CLAUDE_PLUGIN_TUTORIAL.md - Complete tutorial
   - CLAUDE_PLUGIN_6502.md - Plugin reference
   - plugin/README.md - Plugin overview

2. **Try Examples:**
   - Follow PLUGIN_QUICK_START.md
   - Run code snippets from tutorial
   - Explore different symbol tables

3. **Learn 6502:**
   - Start with simple programs
   - Use debugging features
   - Reference symbol tables

## Getting Help

1. Check `/help` in Claude
2. Review plugin/README.md
3. See Troubleshooting section
4. Check plugin configuration

## Files Included

| File | Purpose |
|------|---------|
| index.js | Main plugin code |
| package.json | Plugin metadata |
| config.json.template | Configuration template |
| README.md | Plugin documentation |
| install.sh | Linux/macOS installer |
| install.bat | Windows installer |
| LICENSE | License information |

## Version Information

- **Plugin Version:** 1.0
- **Compatible with:** Claude (current version)
- **Node.js:** 14.0+
- **6502 Simulator:** v3.0+

## Support

For issues:

1. Verify configuration: `cat config.json | jq .`
2. Test simulator directly: `/path/to/sim6502 --help`
3. Check permissions: `ls -la ~/.claude/plugins/6502-simulator/`
4. Review error messages in Claude

## License

This plugin is proprietary. See LICENSE file for details.

---

**Ready to code?** Start with CLAUDE_PLUGIN_TUTORIAL.md!
