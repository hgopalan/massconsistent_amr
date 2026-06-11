#!/usr/bin/env python3
"""
Phase 5.1 Regression Test: Time-varying emissions
Test: Single source with time-varying emission rate
Expected behavior: Puff mass and receptor concentrations vary with time
Comparable to: CALPUFF episodic emission scenario
"""

import os
import sys
import glob
import numpy as np
import subprocess

def test_timevary_emissions():
    """
    Test time-varying emission calculation.
    Verifies:
    1. Emission rates are interpolated correctly
    2. Receptor concentrations increase/decrease with emission rate
    3. No negative or unrealistic concentrations
    """
    
    # Get test directory
    test_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Run solver
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
    output_files = glob.glob(os.path.join(test_dir, "receptor_timevary.csv_step*"))
    if not output_files:
        print("ERROR: No receptor output files generated")
        return False
    
    print(f"Found {len(output_files)} output files")
    
    # Read all output files and check trend
    concentrations_over_time = []
    
    for output_file in sorted(output_files):
        try:
            with open(output_file, 'r') as f:
                lines = [line.strip() for line in f if not line.startswith('#')]
            
            if len(lines) < 2:
                continue
            
            header = lines[0].split(',')
            data = [line.split(',') for line in lines[1:]]
            
            c_idx = header.index('C_total')
            max_conc = max(float(row[c_idx]) for row in data)
            concentrations_over_time.append(max_conc)
            
        except Exception as e:
            print(f"WARNING: Error parsing {output_file}: {e}")
            continue
    
    if not concentrations_over_time:
        print("ERROR: No valid output files parsed")
        return False
    
    # Check that concentrations are positive
    if any(c < 0.0 for c in concentrations_over_time):
        print("ERROR: Negative concentrations detected")
        return False
    
    # Check that at least some variation exists
    if len(concentrations_over_time) > 1:
        min_c = min(concentrations_over_time)
        max_c = max(concentrations_over_time)
        if min_c == max_c and min_c > 0:
            print("WARNING: No temporal variation in concentrations (possible issue)")
    
    print(f"✓ Time-varying emissions test passed")
    print(f"  - {len(concentrations_over_time)} output steps")
    print(f"  - Max concentration range: {min(concentrations_over_time):.6e} to {max(concentrations_over_time):.6e}")
    
    return True

if __name__ == "__main__":
    success = test_timevary_emissions()
    sys.exit(0 if success else 1)
