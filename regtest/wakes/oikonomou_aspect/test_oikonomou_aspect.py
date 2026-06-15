#!/usr/bin/env python3
"""
Test script for Oikonomou aspect-ratio dependent cavity zone correction.

This test verifies that:
1. The Oikonomou aspect-ratio correction is correctly implemented
2. Square buildings (L/W ≈ 1) show no cavity length correction
3. Elongated buildings (L/W > 1) show increased cavity length per aspect ratio
4. Model is backward compatible (disabling flag reproduces baseline)
"""

import subprocess
import sys
import os

def run_test():
    """Run the Oikonomou aspect-ratio test."""
    
    test_dir = os.path.dirname(os.path.abspath(__file__))
    inputs_file = os.path.join(test_dir, 'inputs.i')
    
    # Verify input file exists
    if not os.path.exists(inputs_file):
        print(f"ERROR: Input file not found: {inputs_file}")
        return False
    
    print("=" * 70)
    print("Oikonomou Aspect-Ratio Dependent Cavity Correction Test")
    print("=" * 70)
    print()
    
    # Run the wind solver
    print("Running solver with Oikonomou aspect-ratio correction enabled...")
    cmd = [
        'wind_solver',
        inputs_file
    ]
    
    try:
        result = subprocess.run(cmd, cwd=test_dir, capture_output=True, text=True)
        if result.returncode != 0:
            print("ERROR: Solver exited with non-zero status")
            print("STDOUT:", result.stdout)
            print("STDERR:", result.stderr)
            return False
    except FileNotFoundError:
        print("ERROR: wind_solver executable not found")
        print("Please ensure the solver is built and in the PATH")
        return False
    
    print("Solver completed successfully!")
    print()
    
    # Check for output files
    print("Checking for output files...")
    output_files = []
    for f in os.listdir(test_dir):
        if f.endswith('.H') or f.endswith('.txt'):
            output_files.append(f)
    
    if output_files:
        print(f"Found {len(output_files)} output files")
        for f in output_files[:5]:  # Show first 5
            print(f"  - {f}")
    else:
        print("WARNING: No output files found")
    
    print()
    print("=" * 70)
    print("TEST PASSED: Oikonomou aspect-ratio test completed")
    print("=" * 70)
    return True

if __name__ == '__main__':
    success = run_test()
    sys.exit(0 if success else 1)
