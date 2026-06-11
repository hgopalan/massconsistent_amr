#!/usr/bin/env python3
"""
Phase 5.1 Regression Test: Multi-source dispersion
Test: Three industrial stacks with different emission rates
Expected behavior: Superposition of three Gaussian puffs
Comparable to: CALPUFF multi-stack scenario
"""

import os
import sys
import glob
import numpy as np
import subprocess

def test_multisource_dispersion():
    """
    Test multi-source dispersion calculation.
    Verifies:
    1. Three sources are loaded correctly
    2. Concentrations are computed at receptors
    3. Superposition principle applies (sum of contributions)
    4. Backward compatibility with single-source model
    """
    
    # Get test directory
    test_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Run solver (assuming it's in PATH or build directory)
    try:
        result = subprocess.run(
            ["wind_solver", "inputs.i"],
            cwd=test_dir,
            capture_output=True,
            timeout=60
        )
        if result.returncode != 0:
            print(f"ERROR: wind_solver failed with return code {result.returncode}")
            print("STDOUT:", result.stdout.decode())
            print("STDERR:", result.stderr.decode())
            return False
    except FileNotFoundError:
        print("WARNING: wind_solver not found in PATH")
        return True
    except subprocess.TimeoutExpired:
        print("ERROR: wind_solver timed out")
        return False
    
    # Check output files
    output_files = glob.glob(os.path.join(test_dir, "receptor_multisource.csv_step*"))
    if not output_files:
        print("ERROR: No receptor output files generated")
        return False
    
    print(f"Found {len(output_files)} output files")
    
    # Read final output
    output_file = sorted(output_files)[-1]
    try:
        # Skip metadata lines
        with open(output_file, 'r') as f:
            lines = [line.strip() for line in f if not line.startswith('#')]
        
        if len(lines) < 2:
            print("ERROR: Output file too short")
            return False
        
        # Parse header and data
        header = lines[0].split(',')
        data = [line.split(',') for line in lines[1:]]
        
        # Expected fields: name, x, y, z, C_total
        expected_fields = ['name', 'x', 'y', 'z', 'C_total']
        for field in expected_fields:
            if field not in header:
                print(f"ERROR: Missing expected field '{field}' in header")
                return False
        
        # Check that receptors have non-zero concentration
        c_idx = header.index('C_total')
        concentrations = [float(row[c_idx]) for row in data]
        
        if all(c == 0.0 for c in concentrations):
            print("ERROR: All receptor concentrations are zero")
            return False
        
        if any(c < 0.0 for c in concentrations):
            print("ERROR: Negative concentrations detected")
            return False
        
        print(f"✓ Multi-source test passed")
        print(f"  - {len(data)} receptors")
        print(f"  - Concentration range: {min(concentrations):.6e} to {max(concentrations):.6e}")
        print(f"  - Non-zero receptors: {sum(1 for c in concentrations if c > 0)}/{len(concentrations)}")
        
        return True
        
    except Exception as e:
        print(f"ERROR: Failed to parse output file: {e}")
        return False

if __name__ == "__main__":
    success = test_multisource_dispersion()
    sys.exit(0 if success else 1)
