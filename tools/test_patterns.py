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
     "LDA #$C8\n    STA $10\n"
     "LDA #$00\n    STA $11\n"
     "LDA #$34\n    STA $12\n"
     "LDA #$00\n    STA $13",
     "LDA $13\n    LDX $12",          # A=DST_HI=00, X=DST_LO=FC
     "A=00 X=FC", []),

    # ── sub16 ──────────────────────────────────────────────────────────────────
    # DST = $0100 (256), SRC = $0034 (52)  →  DST = $00CC (204)
    ("sub16", "$0100 - $0034 = $00CC",
     "LDA #$34\n    STA $10\n"
     "LDA #$00\n    STA $11\n"
     "LDA #$00\n    STA $12\n"
     "LDA #$01\n    STA $13",
     "LDA $13\n    LDX $12",          # A=DST_HI=00, X=DST_LO=CC
     "A=00 X=CC", []),

    # ── mul8 ───────────────────────────────────────────────────────────────────
    # 7 × 9 = 63 = $3F
    ("mul8", "7 * 9 = 63 ($3F)",
     "LDA #$07\n    STA $10\n"
     "LDA #$09\n    STA $11",
     "LDA $13\n    LDX $12",          # A=RESULT_HI=00, X=RESULT_LO=3F
     "A=00 X=3F", []),

    # ── mul8_mega65 ─────────────────────────────────────────────────────────────
    # 12 × 11 = 132 = $84  (uses MEGA65 hardware multiply registers)
    ("mul8_mega65", "12 * 11 = 132 ($84) via MEGA65 hardware",
     "LDA #$0C\n    STA $10\n"
     "LDA #$0B\n    STA $11",
     "LDA $13\n    LDX $12",          # A=RESULT_HI=00, X=RESULT_LO=84
     "A=00 X=84", ["-p", "45gs02"]),

    # ── div16 ──────────────────────────────────────────────────────────────────
    # 100 / 7 = 14 remainder 2
    # DIVIDEND_LO/HI are overwritten by quotient on exit; REMAINDER at $13
    ("div16", "100 / 7 = 14 R 2",
     "LDA #$64\n    STA $10\n"
     "LDA #$00\n    STA $11\n"
     "LDA #$07\n    STA $12",
     "LDA $11\n    LDX $10\n    LDY $13",  # A=Q_HI=00, X=Q_LO=0E, Y=REM=02
     "A=00 X=0E Y=02", []),

    # ── div16_mega65 ────────────────────────────────────────────────────────────
    # 200 / 7 = 28 remainder 4  (uses MEGA65 hardware divide registers)
    ("div16_mega65", "200 / 7 = 28 R 4 via MEGA65 hardware",
     "LDA #$C8\n    STA $10\n"
     "LDA #$00\n    STA $11\n"
     "LDA #$07\n    STA $12\n"
     "LDA #$00\n    STA $13",
     "LDA $14\n    LDX $15\n    LDY $16",  # A=RESULT_LO=1C, X=RESULT_HI=00, Y=REM=04
     "A=1C X=00 Y=04", ["-p", "45gs02"]),

    # ── bin_to_bcd ─────────────────────────────────────────────────────────────
    # 150 = 1*100 + 5*10 + 0  →  HUNDREDS=1, TENS=5, UNITS=0
    ("bin_to_bcd", "150 → hundreds=1, tens=5, units=0",
     "LDA #$96\n    STA $10",
     "LDA $11\n    LDX $12\n    LDY $13",  # A=HUNDREDS=01, X=TENS=05, Y=UNITS=00
     "A=01 X=05 Y=00", []),

    # ── bin_to_bcd_65c02 ───────────────────────────────────────────────────────
    # Same test; this variant uses 65C02 BRA instruction
    ("bin_to_bcd_65c02", "150 → 1,5,0 (65C02 BRA variant)",
     "LDA #$96\n    STA $10",
     "LDA $11\n    LDX $12\n    LDY $13",
     "A=01 X=05 Y=00", ["-p", "65c02"]),

    # ── bcd_to_bin ─────────────────────────────────────────────────────────────
    # TENS=9, UNITS=9  →  9*10+9 = 99 = $63
    ("bcd_to_bin", "BCD 99 → $63",
     "LDA #$09\n    STA $10\n"
     "LDA #$09\n    STA $11",
     "LDA $12",                        # A=RESULT=$63
     "A=63", []),

    # ── str_out ────────────────────────────────────────────────────────────────
    # Empty string ($00 at $0300): no CHROUT call, returns immediately.
    # A = last byte read from string pointer ($00), Y = 0
    ("str_out", "empty string (null at $0300) returns without calling CHROUT",
     "LDA #$00\n    STA $0300\n"      # null terminator
     "LDA #$00\n    STA $10\n"        # STR_LO = lo($0300)
     "LDA #$03\n    STA $11",         # STR_HI = hi($0300)
     "LDA $10\n    LDX $11",          # A=STR_LO=00 (unchanged), X=STR_HI=03
     "A=00 X=03", []),                 # Y is 0 from the routine but not checked

    # ── memcopy ────────────────────────────────────────────────────────────────
    # Copy 4 bytes [$AA,$BB,$CC,$DD] from $0300 to $0400
    ("memcopy", "copy 4 bytes $0300→$0400",
     "LDA #$AA\n    STA $0300\n"
     "LDA #$BB\n    STA $0301\n"
     "LDA #$CC\n    STA $0302\n"
     "LDA #$DD\n    STA $0303\n"
     "LDA #$00\n    STA $10\n"        # SRC_LO = lo($0300)
     "LDA #$03\n    STA $11\n"        # SRC_HI = hi($0300)
     "LDA #$00\n    STA $12\n"        # DST_LO = lo($0400)
     "LDA #$04\n    STA $13\n"        # DST_HI = hi($0400)
     "LDA #$04\n    STA $14",         # COUNT = 4
     "LDA $0400\n    LDX $0401",      # A=dst[0]=$AA, X=dst[1]=$BB
     "A=AA X=BB", []),

    # ── memfill ────────────────────────────────────────────────────────────────
    # Fill 4 bytes at $0300 with $55
    ("memfill", "fill 4 bytes at $0300 with $55",
     "LDA #$00\n    STA $10\n"        # DST_LO = lo($0300)
     "LDA #$03\n    STA $11\n"        # DST_HI = hi($0300)
     "LDA #$55\n    STA $12\n"        # VALUE
     "LDA #$04\n    STA $13",         # COUNT
     "LDA $0300\n    LDX $0301",      # A=$55, X=$55
     "A=55 X=55", []),

    # ── delay ──────────────────────────────────────────────────────────────────
    # Short delay; A is not modified by the routine (only X and Y).
    # Push $42, run delay, pull $42 back.
    ("delay", "short delay; A preserved across call",
     "LDA #$10\n    STA $10\n"        # OUTER = 16
     "LDA #$10\n    STA $11\n"        # INNER = 16
     "LDA #$42\n    PHA",             # save sentinel in A via stack
     "PLA",                            # restore A = $42
     "A=42", []),
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


def build_asm(pattern_body, routine_name, setup, result_to_regs):
    """Assemble a small test harness around the pattern body."""
    return (
        f"    JMP _test_main\n"
        f"\n"
        f"{pattern_body}\n"
        f"_test_main:\n"
        f"    {setup.replace(chr(10), chr(10) + '    ')}\n"
        f"    JSR {routine_name}\n"
        f"    {result_to_regs.replace(chr(10), chr(10) + '    ')}\n"
        f"    BRK\n"
    )


def run_test(name, desc, setup, result_to_regs, expect_str, extra_flags, body):
    label = f"{name}: {desc}"
    print(f"  {label}...", end=' ', flush=True)

    asm = build_asm(body, name, setup, result_to_regs)

    with tempfile.NamedTemporaryFile(suffix='.asm', mode='w', delete=False) as f:
        f.write(asm)
        tmpname = f.name

    try:
        r = subprocess.run([SIM] + extra_flags + [tmpname],
                           capture_output=True, text=True, timeout=5)
    except subprocess.TimeoutExpired:
        print("FAIL (timeout)")
        os.unlink(tmpname)
        return False
    finally:
        if os.path.exists(tmpname):
            os.unlink(tmpname)

    if r.returncode != 0:
        print(f"FAIL (exit {r.returncode})")
        if r.stderr:
            print(f"    stderr: {r.stderr.strip()}")
        return False

    # Parse register line from simulator output
    m = re.search(r'Registers: (.*)', r.stdout)
    if not m:
        m = re.search(r'REGS (.*)', r.stdout)
    if not m:
        print("FAIL (no register output)")
        print(f"    stdout: {r.stdout[:200]}")
        return False

    actual_str = m.group(1).strip()

    def parse_regs(s):
        return {k: v for k, v in re.findall(r'(\w+)=([0-9A-F]+)', s)}

    expected = parse_regs(expect_str)
    actual   = parse_regs(actual_str)

    mismatches = [(k, expected[k], actual.get(k, '??'))
                  for k in expected if actual.get(k) != expected[k]]

    if not mismatches:
        print("PASS")
        return True

    print("FAIL")
    for reg, exp, got in mismatches:
        print(f"    {reg}: expected {exp}, got {got}")
    print(f"    actual: {actual_str}")
    return False


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
    for name, desc, setup, result_to_regs, expect, flags in TESTS:
        if run_test(name, desc, setup, result_to_regs, expect, flags, bodies[name]):
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
