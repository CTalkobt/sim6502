# 6502 Simulator Claude Plugin - Complete Package

## What's Inside

This zip file contains everything you need to install and use the 6502 Simulator plugin in Claude.

### Contents

```
6502-plugin.zip (14 KB)
│
├── plugin/
│   ├── index.js                 Main plugin code (12 KB)
│   ├── package.json             Plugin metadata
│   ├── config.json.template     Configuration template
│   ├── README.md                Plugin documentation
│   ├── LICENSE                  Proprietary license
│   ├── install.sh               Linux/macOS installer script
│   └── install.bat              Windows installer script
│
└── INSTALLATION_GUIDE.md        Complete installation guide
```

## Quick Start (3 Steps)

### 1. Extract the zip file

```bash
unzip 6502-plugin.zip
cd plugin
```

### 2. Run the installer

**Linux/macOS:**
```bash
chmod +x install.sh
./install.sh /path/to/6502-simulator
```

**Windows:**
```cmd
install.bat C:\path\to\6502-simulator
```

### 3. Restart Claude and test

```
/help
```

## What the Plugin Does

✅ **Execute 6502 Assembly** - Run code directly in Claude  
✅ **5 Processor Variants** - 6502, 6502-undoc, 65C02, 65CE02, 45GS02  
✅ **Symbol Tables** - Commodore 64, 128, Mega65, X16  
✅ **Debugging** - Breakpoints, traces, memory views  
✅ **Real-time Output** - See results instantly  

## Example Usage

```
/asm -p 6502

LDA #$42
STA $1000
```

Output shows registers, flags, memory state, and cycle count.

## System Requirements

- **Claude** - Latest version
- **6502 Simulator** - Built from source
- **Basic tools** - Unzip, text editor
- **~15 minutes** - Total installation time

## Installation Methods

### Automated (Recommended)

Use the installer script for your platform:
- **Linux/macOS:** `./install.sh /path/to/simulator`
- **Windows:** `install.bat C:\path\to\simulator`

### Manual

1. Create directory: `~/.claude/plugins/6502-simulator`
2. Copy plugin files to that directory
3. Create and edit `config.json` with your paths
4. Restart Claude

## Configuration

The installer creates `config.json` automatically. If you need to edit it manually:

```json
{
  "simulator_path": "/full/path/to/sim6502",
  "work_directory": "/tmp/6502-work",
  "symbol_paths": {
    "c64": "/full/path/to/symbols/c64.sym",
    "c128": "/full/path/to/symbols/c128.sym",
    "mega65": "/full/path/to/symbols/mega65.sym",
    "x16": "/full/path/to/symbols/x16.sym"
  }
}
```

## Verification

Test that everything works:

1. In Claude, type: `/help`
2. You should see plugin help output
3. Try: `/processors`
4. Try a simple program:
   ```
   /asm -p 6502
   LDA #$42
   STA $1000
   ```

## Troubleshooting

### Plugin doesn't load
- Check `config.json` exists
- Verify paths are correct and absolute
- Restart Claude completely

### Simulator not found
- Verify simulator built: `./sim6502 --help`
- Update `config.json` path
- Check file permissions

### Symbol tables not loading
- Verify symbol files exist
- Check `config.json` paths
- Run `/symbols` to list available

See INSTALLATION_GUIDE.md for detailed troubleshooting.

## File Descriptions

### index.js (Main Plugin)
- Complete plugin implementation
- 400+ lines of production code
- All 5 commands implemented
- Full error handling

### config.json.template
- Configuration template
- Shows all available options
- Comments explain each setting
- Copy to config.json and edit

### install.sh (Linux/macOS)
- Automated installation
- Creates directories
- Sets permissions
- Validates configuration

### install.bat (Windows)
- Windows installer script
- Creates directories
- Generates config.json
- Easy installation

### README.md (Plugin)
- Plugin documentation
- Usage examples
- Configuration guide
- Troubleshooting tips

## Features

### Execution
- Execute any 6502 assembly code
- Supports all addressing modes
- Cycle-accurate simulation
- Complete instruction set

### Processors
- **6502** - Original, 56 instructions
- **6502-undoc** - With undocumented opcodes
- **65C02** - Enhanced version
- **65CE02** - Extended variant
- **45GS02** - Modern (Mega65)

### Symbol Tables
- **c64** - Commodore 64 (150+ symbols)
- **c128** - Commodore 128 (160+ symbols)
- **mega65** - Mega65 (120+ symbols)
- **x16** - Commander X16 (100+ symbols)

### Debugging
- **Breakpoints** - Stop at specific addresses
- **Traces** - See every instruction executed
- **Memory Views** - Inspect memory regions
- **Register Display** - View all CPU state

### Output
- Register state
- Processor flags
- Execution statistics
- Memory writes
- Execution traces

## Documentation

Additional documentation files are available:
- **CLAUDE_PLUGIN_TUTORIAL.md** - Complete tutorial (62 pages)
- **CLAUDE_PLUGIN_6502.md** - Plugin reference
- **README.md** (simulator) - Simulator documentation
- **SYMBOL_TABLE_GUIDE.md** - Symbol reference

## Next Steps After Installation

1. **Read the tutorial** - CLAUDE_PLUGIN_TUTORIAL.md
2. **Try examples** - Run code snippets from tutorial
3. **Explore features** - Test all 5 processors
4. **Learn 6502** - Build real programs
5. **Debug code** - Use breakpoints and traces

## Support

If you encounter issues:

1. Check plugin loads: `/help`
2. Verify config: `cat config.json | jq .`
3. Test simulator: `./sim6502 --help`
4. Review INSTALLATION_GUIDE.md
5. Check plugin README.md

## License

This plugin is proprietary. See LICENSE file for details.

## Version Information

- **Plugin Version:** 1.0
- **Compatible with:** Claude (current and later)
- **Requires:** Node.js 14.0+
- **6502 Simulator:** v3.0+

## Quick Reference

### Commands
```
/asm [options] [code]     Execute assembly
/processors               List CPUs
/symbols                  List symbol tables
/help                     Show help
/version                  Show version
```

### Options
```
-p <cpu>                  Processor (6502, 65c02, etc.)
-s <table>                Symbol table (c64, c128, mega65, x16)
-b <addr>                 Breakpoint (0x1000)
-t                        Enable trace
-m <range>                Memory view (0x1000:0x1010)
```

### Example Commands
```
/asm -p 6502 -s c64
/asm -p 65c02 -b 0x1000 -t
/asm -s mega65 -m 0xd000:0xd030
```

## Getting Started

1. **Extract:** `unzip 6502-plugin.zip`
2. **Install:** `./install.sh /path/to/6502-simulator`
3. **Restart:** Close and reopen Claude
4. **Test:** `/help` in Claude
5. **Learn:** Read CLAUDE_PLUGIN_TUTORIAL.md
6. **Code:** Write your first 6502 program!

---

**Questions?** See INSTALLATION_GUIDE.md for detailed setup instructions.

**Ready to code?** See CLAUDE_PLUGIN_TUTORIAL.md for comprehensive guide.

**Happy 6502 assembly coding in Claude!** 🎉
