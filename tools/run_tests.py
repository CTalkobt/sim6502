#!/usr/bin/env python3
import os
import subprocess
import re
import sys
import shlex

def run_test(asm_file):
    print(f"Running test: {asm_file}...", end=" ", flush=True)

    # Read expectations and optional FLAGS from leading comment lines
    expectations = None
    extra_flags = []
    with open(asm_file, 'r') as f:
        for _ in range(10):
            line = f.readline()
            if not line or not line.startswith(';'):
                break
            m = re.search(r'EXPECT: (.*)', line)
            if m:
                expectations = m.group(1).strip()
            m = re.search(r'FLAGS: (.*)', line)
            if m:
                extra_flags = shlex.split(m.group(1).strip())

    # Run simulator
    try:
        flags = extra_flags
        if 'all_' in asm_file and '-a' not in ' '.join(flags):
            flags = flags + ['-a', '$1000']
            
        result = subprocess.run(['./sim6502'] + flags + [asm_file], capture_output=True, text=True, timeout=5)
    except subprocess.TimeoutExpired:
        print("FAIL (timeout)")
        return False
    
    if result.returncode != 0:
        print(f"FAIL (return code {result.returncode})")
        print(result.stderr)
        return False

    if expectations is None:
        print("PASS (assembly)")
        return True
    
    # Parse actual results
    # First try "Registers: ..." (standard output)
    actual_match = re.search(r'Registers: (.*)', result.stdout)
    if not actual_match:
        # Then try "REGS ..." (45GS02 specific output)
        actual_match = re.search(r'REGS (.*)', result.stdout)
        
    if not actual_match:
        print("FAIL (could not parse results)")
        print(result.stdout)
        return False
    
    actual = actual_match.group(1).strip()
    
    # Compare expectations
    def parse_regs(s):
        return dict(re.findall(r'(\w+)=([\dA-F]+)', s))
    
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
        print("PASS")
        return True
    else:
        print(f"FAIL")
        print(f"  Expected: {expectations}")
        print(f"  Actual:   {actual}")
        for detail in mismatch_details:
            print(f"  - {detail}")
        return False

def main():
    test_dir = 'tests'
    if not os.path.exists(test_dir):
        print(f"Test directory '{test_dir}' not found.")
        return 1
        
    tests = [os.path.join(test_dir, f) for f in os.listdir(test_dir) if f.endswith('.asm')]
    tests.sort()
    
    if not tests:
        print("No tests found.")
        return 0
    
    failed = 0
    for test in tests:
        if not run_test(test):
            failed += 1
            
    print(f"\nSummary: {len(tests) - failed} passed, {failed} failed.")
    return 1 if failed > 0 else 0

if __name__ == "__main__":
    sys.exit(main())
