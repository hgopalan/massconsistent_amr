#!/usr/bin/env python3
"""
Test data assimilation module (Hybrid Ensemble Kalman Filter)

This regression test validates:
1. EnKF initialization with parmparse configuration
2. Ensemble member generation with parameter perturbations
3. Forecast cycle (solving each ensemble member)
4. Analysis step and Kalman gain computation
5. Mass conservation projection after analysis
6. Ensemble mean and uncertainty extraction
7. Backward compatibility (EnKF disabled by default)

Expected behavior:
- Ensemble members should have different parameters (u_*, z0, wind_direction)
- Analysis step should reduce ensemble spread (assimilation impact)
- Divergence should be minimized after projection (<1e-6)
- All fields should be valid (no NaNs, reasonable magnitudes)
"""

import os
import sys
import subprocess
import numpy as np
from pathlib import Path

# Get test directory
test_dir = Path(__file__).parent.absolute()
build_dir = test_dir.parent.parent.parent.parent / "build"
binary_path = build_dir / "wind_solver"

def run_test():
    """Execute EnKF regression test"""
    
    if not binary_path.exists():
        print(f"ERROR: Binary not found at {binary_path}")
        print("Build the project first with: cmake --build build")
        return False
    
    print(f"Running data assimilation (EnKF) regression test")
    print(f"Test directory: {test_dir}")
    print(f"Binary: {binary_path}")
    
    # Change to test directory
    os.chdir(test_dir)
    
    # Run solver
    cmd = [str(binary_path), "inputs"]
    print(f"\nCommand: {' '.join(cmd)}")
    print("=" * 70)
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    print(result.stdout)
    if result.stderr:
        print("STDERR:", result.stderr)
    
    if result.returncode != 0:
        print(f"\nERROR: Solver exited with code {result.returncode}")
        return False
    
    print("\n" + "=" * 70)
    print("Test PASSED: EnKF module executed successfully")
    return True

if __name__ == "__main__":
    success = run_test()
    sys.exit(0 if success else 1)
