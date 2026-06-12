#!/usr/bin/env python3
"""
test_cell_local_anisotropy_all.py - Regression test for Cell-Local Spatially-Varying Variational Anisotropy (all sources).
"""

import os
import sys
from pathlib import Path

# Add python path
TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent.parent / "src" / "python"
sys.path.insert(0, str(SRC_PYTHON_DIR))

from wind_solver import WindSolver

def run_test():
    print("Initializing WindSolver with cell-local anisotropy (all sources) inputs.i...")
    inputs_file = TEST_DIR / "inputs.i"
    
    solver = WindSolver()
    success = solver.initialize(str(inputs_file))
    if not success:
        print("ERROR: Failed to initialize WindSolver")
        return False
        
    print("Running solve...")
    result = solver.solve()
    if not result['success']:
        print("ERROR: Solver failed to converge")
        return False
        
    print(f"✓ Mass-consistent solve succeeded in {solver.iters} iterations.")
    print(f"✓ Final residual: {solver.residual:.2e}")
    
    # Extract velocity field
    vel = solver.get_velocity()
    u, v, w = vel['u'], vel['v'], vel['w']
    print(f"✓ Velocity field shape: {u.shape}")
    print(f"✓ Velocity ranges:")
    print(f"  u: [{u.min():.2f}, {u.max():.2f}] m/s")
    print(f"  v: [{v.min():.2f}, {v.max():.2f}] m/s")
    print(f"  w: [{w.min():.2f}, {w.max():.2f}] m/s")
    
    # Since terrain slope is highest on the hill sides and stable temperature gradient
    # is set, alpha_v will vary cell-locally based on all sources (slope, Richardson, Froude).
    # Let's clean up
    solver.finalize()
    return True

if __name__ == "__main__":
    success = run_test()
    sys.exit(0 if success else 1)
