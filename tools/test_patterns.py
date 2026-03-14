#!/usr/bin/env python3
"""
Validate every built-in pattern snippet by assembling and running a small
test harness against the simulator.

Each test case:
  1. Fetches the pattern body from the simulator (get_pattern command).
  2. Wraps it in a minimal .asm program:
       JMP _test_main
       <pattern body>
     _test_main:
       <setup: store inputs into ZP>
       JSR <routine>
       <result: load outputs into A/X/Y>
       BRK
  3. Runs ./sim6502 [-p <proc>] <tmpfile> and checks register expectations.

Pattern equate addresses used in setup/result code must match the defaults
defined inside each pattern body.
"""

import json
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed

MAX_WORKERS = 4

SIM = os.path.join(os.path.dirname(__file__), '..', 'sim6502')

# ── Test case table ────────────────────────────────────────────────────────────
# Columns: pattern_name, description, setup_asm, result_to_regs_asm,
#          expect_str ("REG=XX ..."), extra_flags
#
# Addresses in setup/result use the ZP equates defined in each pattern:
#   add16/sub16 : SRC_LO=$10, SRC_HI=$11, DST_LO=$12, DST_HI=$13
#   mul8        : FACTOR_A=$10, FACTOR_B=$11, RESULT_LO=$12, RESULT_HI=$13
#   div16       : DIVIDEND_LO=$10/$11, DIVISOR=$12, REMAINDER=$13
#   div16_mega65: DIVIDEND=$10/$11, DIVISOR=$12/$13, RESULT=$14/$15, REM=$16
#   bin_to_bcd  : INPUT=$10, HUNDREDS=$11, TENS=$12, UNITS=$13
#   bcd_to_bin  : TENS=$10, UNITS=$11, RESULT=$12
#   str_out     : STR_LO=$10, STR_HI=$11
#   memcopy     : SRC_LO=$10/$11, DST_LO=$12/$13, COUNT=$14
#   memfill     : DST_LO=$10/$11, VALUE=$12, COUNT=$13
#   delay       : OUTER=$10, INNER=$11

TESTS = [
    # ── add16 ──────────────────────────────────────────────────────────────────
    # SRC = $00C8 (200), DST = $0034 (52)  →  DST = $00FC (252)
    ("add16", "200 + 52 = 252 ($00FC)",
     "lda #$C8\n    sta $10\n"
     "lda #$00\n    sta $11\n"
     "lda #$34\n    sta $12\n"
     "lda #$00\n    sta $13",
     "lda $13\n    ldx $12",          # A=DST_HI=00, X=DST_LO=FC
     "A=00 X=FC", []),

    # ── sub16 ──────────────────────────────────────────────────────────────────
    # DST = $0100 (256), SRC = $0034 (52)  →  DST = $00CC (204)
    ("sub16", "$0100 - $0034 = $00CC",
     "lda #$34\n    sta $10\n"
     "lda #$00\n    sta $11\n"
     "lda #$00\n    sta $12\n"
     "lda #$01\n    sta $13",
     "lda $13\n    ldx $12",          # A=DST_HI=00, X=DST_LO=CC
     "A=00 X=CC", []),

    # ── mul8 ───────────────────────────────────────────────────────────────────
    # 7 × 9 = 63 = $3F
    ("mul8", "7 * 9 = 63 ($3F)",
     "lda #$07\n    sta $10\n"
     "lda #$09\n    sta $11",
     "lda $13\n    ldx $12",          # A=RESULT_HI=00, X=RESULT_LO=3F
     "A=00 X=3F", []),

    # ── mul8_mega65 ─────────────────────────────────────────────────────────────
    # 12 × 11 = 132 = $84  (uses MEGA65 hardware multiply registers)
    ("mul8_mega65", "12 * 11 = 132 ($84) via MEGA65 hardware",
     "lda #$0C\n    sta $10\n"
     "lda #$0B\n    sta $11",
     "lda $13\n    ldx $12",          # A=RESULT_HI=00, X=RESULT_LO=84
     "A=00 X=84", ["-M", "mega65"]),

    # ── div16 ──────────────────────────────────────────────────────────────────
    # 100 / 7 = 14 remainder 2
    # DIVIDEND_LO/HI are overwritten by quotient on exit; REMAINDER at $13
    ("div16", "100 / 7 = 14 R 2",
     "lda #$64\n    sta $10\n"
     "lda #$00\n    sta $11\n"
     "lda #$07\n    sta $12",
     "lda $11\n    ldx $10\n    ldy $13",  # A=Q_HI=00, X=Q_LO=0E, Y=REM=02
     "A=00 X=0E Y=02", []),

    # ── div16_mega65 ────────────────────────────────────────────────────────────
    # 200 / 7 = 28 remainder 4  (uses MEGA65 hardware divide registers)
    ("div16_mega65", "200 / 7 = 28 R 4 via MEGA65 hardware",
     "lda #$C8\n    sta $10\n"
     "lda #$00\n    sta $11\n"
     "lda #$07\n    sta $12\n"
     "lda #$00\n    sta $13",
     "lda $14\n    ldx $15\n    ldy $16",  # A=RESULT_LO=1C, X=RESULT_HI=00, Y=REM=04
     "A=1C X=00 Y=04", ["-M", "mega65"]),

    # ── bin_to_bcd ─────────────────────────────────────────────────────────────
    # 150 = 1*100 + 5*10 + 0  →  HUNDREDS=1, TENS=5, UNITS=0
    ("bin_to_bcd", "150 → hundreds=1, tens=5, units=0",
     "lda #$96\n    sta $10",
     "lda $11\n    ldx $12\n    ldy $13",  # A=HUNDREDS=01, X=TENS=05, Y=UNITS=00
     "A=01 X=05 Y=00", []),

    # ── bin_to_bcd_65c02 ───────────────────────────────────────────────────────
    # Same test; this variant uses 65C02 BRA instruction
    ("bin_to_bcd_65c02", "150 → 1,5,0 (65C02 BRA variant)",
     "lda #$96\n    sta $10",
     "lda $11\n    ldx $12\n    ldy $13",
     "A=01 X=05 Y=00", ["-p", "65c02"]),

    # ── bcd_to_bin ─────────────────────────────────────────────────────────────
    # TENS=9, UNITS=9  →  9*10+9 = 99 = $63
    ("bcd_to_bin", "BCD 99 → $63",
     "lda #$09\n    sta $10\n"
     "lda #$09\n    sta $11",
     "lda $12",                        # A=RESULT=$63
     "A=63", []),

    # ── str_out ────────────────────────────────────────────────────────────────
    # Empty string ($00 at $0300): no CHROUT call, returns immediately.
    # A = last byte read from string pointer ($00), Y = 0
    ("str_out", "empty string (null at $0300) returns without calling CHROUT",
     "lda #$00\n    sta $0300\n"      # null terminator
     "lda #$00\n    sta $10\n"        # STR_LO = lo($0300)
     "lda #$03\n    sta $11",         # STR_HI = hi($0300)
     "lda $10\n    ldx $11",          # A=STR_LO=00 (unchanged), X=STR_HI=03
     "A=00 X=03", []),                 # Y is 0 from the routine but not checked

    # ── memcopy ────────────────────────────────────────────────────────────────
    # Copy 4 bytes [$AA,$BB,$CC,$DD] from $0300 to $0400
    ("memcopy", "copy 4 bytes $0300→$0400",
     "lda #$AA\n    sta $0300\n"
     "lda #$BB\n    sta $0301\n"
     "lda #$CC\n    sta $0302\n"
     "lda #$DD\n    sta $0303\n"
     "lda #$00\n    sta $10\n"        # SRC_LO = lo($0300)
     "lda #$03\n    sta $11\n"        # SRC_HI = hi($0300)
     "lda #$00\n    sta $12\n"        # DST_LO = lo($0400)
     "lda #$04\n    sta $13\n"        # DST_HI = hi($0400)
     "lda #$04\n    sta $14",         # COUNT = 4
     "lda $0400\n    ldx $0401",      # A=dst[0]=$AA, X=dst[1]=$BB
     "A=AA X=BB", []),

    # ── memfill ────────────────────────────────────────────────────────────────
    # Fill 4 bytes at $0300 with $55
    ("memfill", "fill 4 bytes at $0300 with $55",
     "lda #$00\n    sta $10\n"        # DST_LO = lo($0300)
     "lda #$03\n    sta $11\n"        # DST_HI = hi($0300)
     "lda #$55\n    sta $12\n"        # VALUE
     "lda #$04\n    sta $13",         # COUNT
     "lda $0300\n    ldx $0301",      # A=$55, X=$55
     "A=55 X=55", []),

    # ── delay ──────────────────────────────────────────────────────────────────
    # Short delay; A is not modified by the routine (only X and Y).
    # Push $42, run delay, pull $42 back.
    ("delay", "short delay; A preserved across call",
     "lda #$10\n    sta $10\n"        # OUTER = 16
     "lda #$10\n    sta $11\n"        # INNER = 16
     "lda #$42\n    pha",             # save sentinel in A via stack
     "pla",                            # restore A = $42
     "A=42", []),

    # ── bitmap_init ────────────────────────────────────────────────────────────
    ("bitmap_init", "initialize bitmap mode and screen RAM",
     "",
     "lda $D011\n    ldx $D016\n    ldy $0400",
     "A=3B X=C8 Y=10", []),

    # ── bitmap_plot ────────────────────────────────────────────────────────────
    ("bitmap_plot", "plot pixel (0,0) - set",
     "lda #0\n    sta $14\n"         # PX=0
     "lda #0\n    sta $15\n"         # PY=0
     "lda #1\n    sta $19\n"         # PLOT_OP=1 (set)
     "lda #0\n    sta $2000\n"       # clear target byte
     "lda #$00\n  sta $4000\n"       # row_base_lo[0] = $00
     "lda #$20\n  sta $4019\n"       # row_base_hi[0] = $20
     "lda #$80\n  sta $4032",        # bitmask_table[0] = $80
     "lda $2000",                    # should be $80
     "A=80", []),

    # ── bitmap_setcell ─────────────────────────────────────────────────────────
    ("bitmap_setcell", "set cell (0,0) to color $10",
     "lda #0\n    sta $10\n"         # CELL_X=0
     "lda #0\n    sta $11\n"         # CELL_Y=0
     "lda #1\n    sta $12\n"         # FG_COL=1
     "lda #0\n    sta $13",          # BG_COL=0
     "lda $0400",                    # should be $10
     "A=10", []),

    # ── bitmap_clear ───────────────────────────────────────────────────────────
    ("bitmap_clear", "clear bitmap (software)",
     "lda #$ff\n    sta $2000\n"
     "sta $3f3f",
     "lda $2000",
     "A=00", []),

    # ── bitmap_clear_mega65 ────────────────────────────────────────────────────
    ("bitmap_clear_mega65", "clear bitmap (DMA)",
     "lda #$FF\n    sta $2000\n"
     "sta $3F3F",
     "lda $2000\n    ldx $3F3F",
     "A=00 X=00", ["-M", "mega65"]),

    # ── dma_copy ───────────────────────────────────────────────────────────────
    ("dma_copy", "copy 4 bytes via DMA",
     "lda #$AA\n    sta $0300\n"
     "lda #$BB\n    sta $0301\n"
     "lda #$CC\n    sta $0302\n"
     "lda #$DD\n    sta $0303\n"
     "lda #$00\n    sta $10\n"        # SRC_LO
     "lda #$03\n    sta $11\n"        # SRC_HI
     "lda #$00\n    sta $12\n"        # DST_LO
     "lda #$04\n    sta $13\n"        # DST_HI
     "lda #$04\n    sta $14\n"        # CNT_LO
     "lda #$00\n    sta $15",         # CNT_HI
     "lda $0400\n    ldx $0401",
     "A=AA X=BB", ["-M", "mega65"]),

    # ── dma_fill ───────────────────────────────────────────────────────────────
    ("dma_fill", "fill 4 bytes via DMA",
     "lda #$55\n    sta $10\n"        # VALUE
     "lda #$00\n    sta $11\n"        # DST_LO
     "lda #$03\n    sta $12\n"        # DST_HI
     "lda #$04\n    sta $13\n"        # CNT_LO
     "lda #$00\n    sta $14",         # CNT_HI
     "lda $0300\n    ldx $0301",
     "A=55 X=55", ["-M", "mega65"]),

    # ── sprite_init ────────────────────────────────────────────────────────────
    ("sprite_init", "initialize sprite 0",
     "lda #0\n    sta $10\n"         # SPRITE_NUM=0
     "lda #100\n  sta $11\n"         # SPR_X_LO=100
     "lda #0\n    sta $12\n"         # SPR_X_HI=0
     "lda #50\n   sta $13\n"         # SPR_Y=50
     "lda #1\n    sta $14\n"         # SPR_COLOR=1
     "lda #$80\n  sta $15",          # SPR_PTR=$80
     "lda $D000\n    ldx $D001\n    ldy $D015",
     "A=64 X=32 Y=01", []),

    # ── sprite_move ────────────────────────────────────────────────────────────
    ("sprite_move", "move sprite 0 to (200, 150)",
     "lda #0\n    sta $10\n"         # SPRITE_NUM=0
     "lda #200\n  sta $11\n"         # SPR_X_LO=200
     "lda #0\n    sta $12\n"         # SPR_X_HI=0
     "lda #150\n  sta $13",          # SPR_Y=150
     "lda $D000\n    ldx $D001",
     "A=C8 X=96", []),

    # ── sprite_setcolor ────────────────────────────────────────────────────────
    ("sprite_setcolor", "set sprite 0 color to 7",
     "lda #0\n    sta $10\n"         # SPRITE_NUM=0
     "lda #7\n    sta $11",          # SPR_COLOR=7
     "lda $D027",
     "A=07", []),

    # ── sprite_mc_init ─────────────────────────────────────────────────────────
    ("sprite_mc_init", "enable multicolor for sprite 0",
     "lda #0\n    sta $10\n"         # SPRITE_NUM=0
     "lda #1\n    sta $11\n"         # SPR_COLOR=1
     "lda #2\n    sta $12\n"         # MC_COLOR0=2
     "lda #3\n    sta $13",          # MC_COLOR1=3
     "lda $D01C\n    ldx $D025\n    ldy $D026",
     "A=01 X=02 Y=03", []),

    # ── sprite_mega65_16col ────────────────────────────────────────────────────
    ("sprite_mega65_16col", "enable 16-color mode for sprite 0",
     "lda #0\n    sta $10",          # SPRITE_NUM=0
     "lda $D04B",
     "A=01", ["-M", "mega65"]),
]

# ── Helpers ────────────────────────────────────────────────────────────────────

def fetch_all_bodies():
    """Start the simulator once and pull every pattern body via get_pattern."""
    tmpasm = "BRK\n"
    with tempfile.NamedTemporaryFile(suffix='.asm', mode='w', delete=False) as f:
        f.write(tmpasm)
        tmpname = f.name

    names = list({t[0] for t in TESTS})
    cmds  = '\n'.join(f"get_pattern {n}" for n in names) + '\nquit\n'

    try:
        r = subprocess.run([SIM, '-I', '-J', tmpname],
                           input=cmds, capture_output=True, text=True, timeout=10)
    except subprocess.TimeoutExpired:
        print("FATAL: simulator timed out while fetching patterns", file=sys.stderr)
        os.unlink(tmpname)
        sys.exit(1)
    finally:
        if os.path.exists(tmpname):
            os.unlink(tmpname)

    bodies = {}
    for raw in r.stdout.splitlines():
        line = raw.strip().lstrip('> ').strip()
        if not line:
            continue
        try:
            d = json.loads(line)
            if d.get('cmd') == 'get_pattern' and d.get('ok'):
                bodies[d['data']['name']] = d['data']['body']
        except (json.JSONDecodeError, KeyError):
            pass
    return bodies


def build_asm(pattern_body, routine_name, setup, result_to_regs, proc="6502"):
    """Assemble a small test harness around the pattern body."""
    cpu_map = {
        "6502": "_6502",
        "65c02": "_65c02",
        "65ce02": "_65ce02",
        "45gs02": "_45gs02"
    }
    cpu = cpu_map.get(proc, "_6502")
    return (
        f".cpu {cpu}\n"
        f"* = $2000\n"
        f"    jmp _test_main\n"
        f"\n"
        f"{pattern_body}\n"
        f"_test_main:\n"
        f"    {setup.replace(chr(10), chr(10) + '    ')}\n"
        f"    jsr {routine_name}\n"
        f"    {result_to_regs.replace(chr(10), chr(10) + '    ')}\n"
        f"    rts\n"
    )


def run_test(name, desc, setup, result_to_regs, expect_str, extra_flags, body):
    """Returns (passed: bool, output: str)."""
    label = f"{name}: {desc}"
    out = [f"  {label}..."]

    proc = "6502"
    if "-p" in extra_flags:
        proc = extra_flags[extra_flags.index("-p") + 1]
    elif "-M" in extra_flags and extra_flags[extra_flags.index("-M") + 1] == "mega65":
        proc = "45gs02"

    asm = build_asm(body, name, setup.lower(), result_to_regs.lower(), proc)

    with tempfile.NamedTemporaryFile(suffix='.asm', mode='w', delete=False) as f:
        f.write(asm)
        tmpname = f.name

    base = os.path.splitext(tmpname)[0]
    prg_name = base + ".prg"

    try:
        # Assemble
        asm_r = subprocess.run(['java', '-jar', 'tools/KickAss65CE02.jar', tmpname, '-o', prg_name],
                               capture_output=True, text=True, timeout=10)
        if asm_r.returncode != 0:
            out.append("FAIL (assembly)")
            out.append(f"    stdout: {asm_r.stdout.strip()}")
            out.append(f"    stderr: {asm_r.stderr.strip()}")
            return False, '\n'.join(out)

        # Run
        r = subprocess.run([SIM] + extra_flags + [prg_name],
                           capture_output=True, text=True, timeout=5)
    except subprocess.TimeoutExpired:
        out.append("FAIL (timeout)")
        return False, '\n'.join(out)
    finally:
        for f in [tmpname, prg_name]:
            if os.path.exists(f):
                os.unlink(f)

    if r.returncode != 0:
        out.append(f"FAIL (exit {r.returncode})")
        if r.stderr:
            out.append(f"    stderr: {r.stderr.strip()}")
        return False, '\n'.join(out)

    # Parse register line from simulator output
    m = re.search(r'Registers: (.*)', r.stdout)
    if not m:
        m = re.search(r'REGS (.*)', r.stdout)
    if not m:
        out.append("FAIL (no register output)")
        out.append(f"    stdout: {r.stdout[:200]}")
        return False, '\n'.join(out)

    actual_str = m.group(1).strip()

    def parse_regs(s):
        return {k: v for k, v in re.findall(r'(\w+)=([0-9A-F]+)', s)}

    expected = parse_regs(expect_str)
    actual   = parse_regs(actual_str)

    mismatches = [(k, expected[k], actual.get(k, '??'))
                  for k in expected if actual.get(k) != expected[k]]

    if not mismatches:
        out.append("PASS")
        return True, '\n'.join(out)

    out.append("FAIL")
    for reg, exp, got in mismatches:
        out.append(f"    {reg}: expected {exp}, got {got}")
    out.append(f"    actual: {actual_str}")
    return False, '\n'.join(out)


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    if not os.path.isfile(SIM):
        print(f"error: simulator not found at {SIM} — run 'make' first",
              file=sys.stderr)
        return 1

    print("Fetching pattern bodies from simulator...")
    bodies = fetch_all_bodies()

    missing = [t[0] for t in TESTS if t[0] not in bodies]
    if missing:
        print(f"error: patterns not found: {', '.join(missing)}", file=sys.stderr)
        return 1
    print(f"  {len(bodies)} pattern(s) fetched.\n")

    print("Running pattern tests:")
    passed = failed = 0
    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        futures = {
            executor.submit(run_test, name, desc, setup, result_to_regs, expect, flags, bodies[name]): name
            for name, desc, setup, result_to_regs, expect, flags in TESTS
        }
        for future in as_completed(futures):
            ok, output = future.result()
            print(output)
            if ok:
                passed += 1
            else:
                failed += 1

    total = passed + failed
    print(f"\nPattern tests: {passed}/{total} passed.", end='')
    if failed:
        print(f"  {failed} FAILED.")
    else:
        print()
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
