#!/usr/bin/env python3
"""
Phase 5 Master Test Runner
Runs all regression tests and validation suite
"""

import os
import sys
import glob
import subprocess
from pathlib import Path

def run_test(test_script, test_name):
    """Run a single test script and report results."""
    print(f"\n{'='*70}")
    print(f"Running: {test_name}")
    print('='*70)
    
    try:
        result = subprocess.run(
            ["python3", test_script],
            capture_output=True,
            timeout=300
        )
        
        # Print output
        if result.stdout:
            print(result.stdout.decode())
        if result.stderr:
            print("STDERR:", result.stderr.decode())
        
        return result.returncode == 0
        
    except subprocess.TimeoutExpired:
        print(f"ERROR: {test_name} timed out")
        return False
    except FileNotFoundError:
        print(f"ERROR: Python3 not found")
        return False
    except Exception as e:
        print(f"ERROR: {e}")
        return False

def main():
    """Run all Phase 5 tests."""
    
    print("="*70)
    print("Phase 5: Testing & Validation")
    print("="*70)
    
    test_dir = Path(__file__).parent
    results = {}
    
    # Phase 5.1: Regression Tests
    print("\n" + "="*70)
    print("Phase 5.1: Regression Tests")
    print("="*70)
    
    regression_tests = [
        ("dispersion/puff_multisource_three_stacks/test_multisource.py", 
         "5.1a: Multi-source dispersion"),
        ("dispersion/puff_timevary_emissions/test_timevary.py",
         "5.1b: Time-varying emissions"),
        ("dispersion/puff_chemistry_reactions/test_chemistry.py",
         "5.1c: Reactive chemistry"),
    ]
    
    for test_script, test_name in regression_tests:
        test_path = test_dir / test_script
        if test_path.exists():
            results[test_name] = run_test(str(test_path), test_name)
        else:
            print(f"WARNING: {test_script} not found")
            results[test_name] = False
    
    # Phase 5.2: Backwards Compatibility
    print("\n" + "="*70)
    print("Phase 5.2: Backwards Compatibility")
    print("="*70)
    
    compat_test = test_dir / "compatibility/test_backwards_compat.py"
    if compat_test.exists():
        results["5.2: Backwards compatibility"] = run_test(str(compat_test), 
                                                          "5.2: Backwards compatibility")
    else:
        print(f"WARNING: {compat_test} not found")
        results["5.2: Backwards compatibility"] = False
    
    # Print summary
    print("\n" + "="*70)
    print("Test Summary")
    print("="*70)
    
    passed = sum(1 for v in results.values() if v)
    failed = sum(1 for v in results.values() if not v)
    
    for test_name, result in results.items():
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"  {status}: {test_name}")
    
    print(f"\nTotal: {passed} passed, {failed} failed")
    
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
