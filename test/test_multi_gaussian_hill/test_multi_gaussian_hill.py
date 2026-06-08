#!/usr/bin/env python3
"""
test_multi_gaussian_hill.py - Synthetic Multi-Gaussian Hill Test

Tests mass-consistent wind solver using synthetic terrain option.
"""

import os
import sys
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent.parent / "src" / "python"
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    sys.exit(1)


def test_synthetic_terrain():
    print("\n" + "="*70)
    print("Testing Synthetic Multi-Gaussian Hill Terrain")
    print("="*70)
    
    try:
        wind = WindSolver()
        inputs_file = TEST_DIR / "inputs.i"
        wind.initialize(str(inputs_file))
        
        # Verify grid parameters
        print(f"✓ Grid dimensions: {wind.nx}x{wind.ny}x{wind.nz}")
        print(f"✓ Domain bounds:")
        print(f"  X: [{wind.xmin:.1f}, {wind.xmax:.1f}] m")
        print(f"  Y: [{wind.ymin:.1f}, {wind.ymax:.1f}] m")
        print(f"  Z: [{wind.zmin:.1f}, {wind.zmax:.1f}] m")
        
        # Get terrain elevation map
        terrain = wind.get_terrain()
        print(f"✓ Terrain shape: {terrain.shape}")
        print(f"✓ Terrain bounds: [{terrain.min():.2f}, {terrain.max():.2f}] m")
        
        # Maximum elevation should be close to 50m (primary peak is at (100, 150))
        if terrain.max() < 40.0 or terrain.max() > 55.0:
            print(f"ERROR: Synthetic terrain peak height {terrain.max():.2f} is out of expected range")
            return False
            
        print("Solving wind field on synthetic multi-gaussian hill...")
        result = wind.solve()
        
        if not result['success']:
            print("ERROR: Wind solve failed")
            return False
            
        print("✓ Wind solve succeeded!")
        print(f"  MLMG iterations: {wind.iters}")
        print(f"  Final residual: {wind.residual:.2e}")
        
        wind.finalize()
        return True
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


if __name__ == "__main__":
    os.chdir(TEST_DIR)
    success = test_synthetic_terrain()
    sys.exit(0 if success else 1)
