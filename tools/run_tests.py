#!/usr/bin/env python3
import os
import subprocess
import re
import sys
import shlex
from concurrent.futures import ThreadPoolExecutor, as_completed

MAX_WORKERS = 4

def run_test(asm_file):
    """Returns (passed: bool, output: str)."""
    out = [f"Running test: {asm_file}..."]

    # Read expectations, FLAGS, and CPU directive from file
    expectations = None
    extra_flags = []
    cpu_flag = []
    cpu_map = {
        '_45gs02': ['-p', '45gs02'],
        '_65ce02': ['-p', '65ce02'],
        '_65c02':  ['-p', '65c02'],
        '_6502':   ['-p', '6502'],
    }
    with open(asm_file, 'r') as f:
        for _ in range(30):
            line = f.readline()
            if not line:
                break
            m = re.search(r'EXPECT: (.*)', line)
            if m:
                expectations = m.group(1).strip()
            m = re.search(r'FLAGS: (.*)', line)
            if m:
                extra_flags = shlex.split(m.group(1).strip())
            m = re.search(r'\.cpu\s+(\S+)', line)
            if m and not cpu_flag:
                directive = m.group(1).strip().rstrip(';')
                if directive in cpu_map:
                    cpu_flag = cpu_map[directive]

    # Run assembler first (KickAssembler)
    base = os.path.splitext(asm_file)[0]
    prg_file = base + ".prg"
    sym_file = base + ".sym"
    
    try:
        # We use -symbolfile to generate symbols for the simulator
        asm_result = subprocess.run(['java', '-jar', 'tools/KickAss65CE02.jar', asm_file, '-symbolfile', '-o', prg_file],
                                   capture_output=True, text=True, timeout=10)
        if asm_result.returncode != 0:
            out.append("FAIL (assembly error)")
            out.append(asm_result.stdout)
            out.append(asm_result.stderr)
            return False, '\n'.join(out)
    except Exception as e:
        out.append(f"FAIL (assembler launch error: {e})")
        return False, '\n'.join(out)

    if expectations is None:
        out.append("PASS (assembly)")
        return True, '\n'.join(out)

    # Run simulator
    try:
        flags = cpu_flag + extra_flags + ['-vv']
        # Use the assembled PRG
        result = subprocess.run(['./sim6502'] + flags + [prg_file], capture_output=True, text=True, timeout=5)
    except subprocess.TimeoutExpired:
        out.append("FAIL (timeout)")
        return False, '\n'.join(out)

    if result.returncode != 0:
        out.append(f"FAIL (return code {result.returncode})")
        out.append(result.stderr)
        return False, '\n'.join(out)

    # Parse actual results
    # First try "Registers: ..." (standard output)
    actual_match = re.search(r'Registers: (.*)', result.stdout)
    if not actual_match:
        # Then try "REGS ..." (45GS02 specific output)
        actual_match = re.search(r'REGS (.*)', result.stdout)

    if not actual_match:
        out.append("FAIL (could not parse results)")
        out.append(result.stdout)
        return False, '\n'.join(out)

    actual = actual_match.group(1).strip()

    # Compare expectations
    def parse_regs(s):
        # Match Key=Value or Key $Value or Key Value
        return dict(re.findall(r'(\w+)[:=$ ]+([\dA-F]+)', s))

    expected_regs = parse_regs(expectations)
    actual_regs = parse_regs(actual)

    passed = True
    mismatch_details = []
    for reg, val in expected_regs.items():
        actual_val = actual_regs.get(reg)
        if actual_val != val:
            passed = False
            mismatch_details.append(f"{reg}: expected {val}, got {actual_val}")

    if passed:
        out.append("PASS")
        return True, '\n'.join(out)
    else:
        out.append("FAIL")
        out.append(f"  Expected: {expectations}")
        out.append(f"  Actual:   {actual}")
        for detail in mismatch_details:
            out.append(f"  - {detail}")
        return False, '\n'.join(out)

def main():
    test_dir = 'tests/code'
    if not os.path.exists(test_dir):
        print(f"Test directory '{test_dir}' not found.")
        return 1
        
    if len(sys.argv) > 1:
        tests = sys.argv[1:]
    else:
        tests = [os.path.join(test_dir, f) for f in os.listdir(test_dir) if f.endswith('.asm')]
        tests.sort()
    
    if not tests:
        print("No tests found.")
        return 0

    failed = 0
    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        futures = {executor.submit(run_test, t): t for t in tests}
        for future in as_completed(futures):
            passed, output = future.result()
            print(output)
            if not passed:
                failed += 1

    print(f"\nSummary: {len(tests) - failed} passed, {failed} failed.")
    return 1 if failed > 0 else 0

if __name__ == "__main__":
    sys.exit(main())
