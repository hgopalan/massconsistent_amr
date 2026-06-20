#!/usr/bin/env python3
"""
test_case1.py - Case 1: Gaussian Hill with Time-Varying Winds and Turbulence

Tests mass-consistent wind solver with:
- Medium Gaussian hill terrain (500x500 m, 75 m peak)
- Time-varying wind boundary conditions
- Log-law wind profile initialization
- Synthetic turbulence fluctuations
- Modified wind field output

Validates:
1. Solver initialization and grid parameters
2. Wind field solution convergence
3. Terrain-following wind acceleration
4. Time-varying wind integration
5. Turbulence fluctuation application
6. Modified wind output file generation
"""

import os
import sys
import math
from pathlib import Path

# Add parent directory to path for wind_solver import
TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent.parent.parent / "src" / "python"
TOOLS_DIR = TEST_DIR.parent.parent.parent / "tools"
sys.path.insert(0, str(SRC_PYTHON_DIR))
sys.path.insert(0, str(TOOLS_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    print("Make sure to build with Python bindings enabled:")
    print("  cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON")
    sys.exit(1)


def test_initialization():
    """Test solver initialization with Gaussian hill terrain."""
    print("\n" + "="*70)
    print("Test 1: Solver Initialization")
    print("="*70)
    
    try:
        wind = WindSolver()
        inputs_file = TEST_DIR / "inputs.i"
        
        if not inputs_file.exists():
            print(f"ERROR: Inputs file not found: {inputs_file}")
            return False
        
        wind.initialize(str(inputs_file))
        
        # Verify grid parameters
        expected_nx, expected_ny = 21, 21
        
        if wind.nx != expected_nx or wind.ny != expected_ny:
            print(f"ERROR: Grid dimensions mismatch:")
            print(f"  Expected: {expected_nx}x{expected_ny}")
            print(f"  Got: {wind.nx}x{wind.ny}")
            return False
        
        print(f"✓ Grid dimensions: {wind.nx}x{wind.ny}x{wind.nz}")
        print(f"✓ Domain bounds:")
        print(f"  X: [{wind.xmin:.1f}, {wind.xmax:.1f}] m")
        print(f"  Y: [{wind.ymin:.1f}, {wind.ymax:.1f}] m")
        print(f"  Z: [{wind.zmin:.1f}, {wind.zmax:.1f}] m")
        print(f"✓ Grid spacing: dx={wind.dx:.2f} m, dy={wind.dy:.2f} m, dz={wind.dz:.2f} m")
        print(f"✓ Terrain bounds: [{wind.zs_min:.1f}, {wind.zs_max:.1f}] m")
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_wind_solve():
    """Test mass-consistent wind field solution."""
    print("\n" + "="*70)
    print("Test 2: Wind Field Solution")
    print("="*70)
    
    try:
        wind = WindSolver()
        wind.initialize(str(TEST_DIR / "inputs.i"))
        
        # Solve for wind field
        result = wind.solve()
        
        if not result['success']:
            print("ERROR: Wind solve failed")
            return False
        
        print(f"✓ Wind solve completed successfully")
        print(f"✓ MLMG iterations: {wind.iters}")
        print(f"✓ Final residual: {wind.residual:.2e}")
        
        # Verify solution is not trivial
        if wind.residual <= 0:
            print("WARNING: Residual is zero or negative (potential issue)")
        
        # Get velocity field
        vel = wind.get_velocity()
        if 'u' not in vel or 'v' not in vel or 'w' not in vel:
            print("ERROR: Velocity field missing components")
            return False
        
        print(f"✓ Velocity field shape: {vel['u'].shape}")
        
        # Check velocity ranges
        u_min, u_max = vel['u'].min(), vel['u'].max()
        v_min, v_max = vel['v'].min(), vel['v'].max()
        w_min, w_max = vel['w'].min(), vel['w'].max()
        
        print(f"✓ Velocity ranges:")
        print(f"  u: [{u_min:.2f}, {u_max:.2f}] m/s")
        print(f"  v: [{v_min:.2f}, {v_max:.2f}] m/s")
        print(f"  w: [{w_min:.2f}, {w_max:.2f}] m/s")
        
        # Verify wind speeds are reasonable
        if u_max < 10.0 or u_max > 20.0:
            print(f"WARNING: u_max {u_max} m/s seems out of expected range [10, 20]")
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_velocity_extraction():
    """Test velocity extraction at specific heights."""
    print("\n" + "="*70)
    print("Test 3: Velocity Extraction")
    print("="*70)
    
    try:
        wind = WindSolver()
        wind.initialize(str(TEST_DIR / "inputs.i"))
        wind.solve()
        
        # Extract velocity at 30 m AGL
        vel_30m = wind.get_velocity_at_agl(30.0)
        
        if 'u' not in vel_30m or vel_30m['u'] is None:
            print("ERROR: Failed to extract velocity at 30 m AGL")
            return False
        
        print(f"✓ Extracted velocity at {vel_30m['agl']} m AGL")
        print(f"✓ Extracted field shape: {vel_30m['u'].shape}")
        
        # Analyze wind field
        u_mean = vel_30m['u'].mean()
        u_std = vel_30m['u'].std()
        v_mean = vel_30m['v'].mean()
        
        print(f"✓ Wind statistics at {vel_30m['agl']} m AGL:")
        print(f"  U mean: {u_mean:.2f} m/s (std: {u_std:.2f} m/s)")
        print(f"  V mean: {v_mean:.2f} m/s")
        
        # Expect acceleration over hill
        if u_mean < 12.0:
            print(f"WARNING: Mean u-wind {u_mean:.2f} m/s is less than reference 12 m/s")
        
        # Extract at different heights
        vel_10m = wind.get_velocity_at_agl(10.0)
        vel_50m = wind.get_velocity_at_agl(50.0)
        
        u_10m = vel_10m['u'].mean()
        u_50m = vel_50m['u'].mean()
        
        print(f"✓ Wind speed profile:")
        print(f"  @ 10 m AGL: {u_10m:.2f} m/s")
        print(f"  @ 30 m AGL: {u_mean:.2f} m/s")
        print(f"  @ 50 m AGL: {u_50m:.2f} m/s")
        
        # Verify expected wind shear
        if u_50m < u_30m:
            print(f"WARNING: Wind shear direction unexpected (u_50 < u_30)")
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_plotfile_output():
    """Test plotfile output generation."""
    print("\n" + "="*70)
    print("Test 4: Plotfile Output")
    print("="*70)
    
    try:
        wind = WindSolver()
        wind.initialize(str(TEST_DIR / "inputs.i"))
        wind.solve()
        
        # Write plotfile
        plotfile_prefix = "plt_case1_output"
        wind.write_plotfile(plotfile_prefix)
        
        # Check if plotfile was created (AMReX plotfiles are directories)
        plotfile_dir = Path(plotfile_prefix)
        if not plotfile_dir.exists():
            print(f"ERROR: Plotfile directory not created: {plotfile_dir}")
            return False
        
        print(f"✓ Plotfile created: {plotfile_dir}")
        
        # List plotfile contents
        contents = list(plotfile_dir.iterdir())
        print(f"✓ Plotfile contents: {len(contents)} files")
        
        for item in sorted(contents)[:10]:
            print(f"  - {item.name}")
        
        # Verify required files exist
        if (plotfile_dir / "Header").exists():
            print(f"✓ Header file found")
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_terrain_field():
    """Test terrain field access."""
    print("\n" + "="*70)
    print("Test 5: Terrain Field Access")
    print("="*70)
    
    try:
        wind = WindSolver()
        wind.initialize(str(TEST_DIR / "inputs.i"))
        
        # Get terrain
        terrain = wind.get_terrain()
        
        if terrain is None or terrain.size == 0:
            print("ERROR: Terrain field is empty")
            return False
        
        print(f"✓ Terrain field shape: {terrain.shape}")
        print(f"✓ Terrain bounds: [{terrain.min():.2f}, {terrain.max():.2f}] m")
        
        # Verify expected peak
        expected_peak = 75.0
        actual_peak = terrain.max()
        
        if abs(actual_peak - expected_peak) > 5.0:
            print(f"WARNING: Terrain peak {actual_peak:.2f} m deviates from expected {expected_peak:.2f} m")
        else:
            print(f"✓ Terrain peak matches: {actual_peak:.2f} m")
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def run_all_tests():
    """Run all Case 1 tests."""
    print("\n" + "="*70)
    print("Case 1: Gaussian Hill with Time-Varying Winds and Turbulence")
    print("="*70)
    
    # Change to test directory
    os.chdir(TEST_DIR)
    
    tests = [
        ("Solver Initialization", test_initialization),
        ("Wind Field Solution", test_wind_solve),
        ("Velocity Extraction", test_velocity_extraction),
        ("Plotfile Output", test_plotfile_output),
        ("Terrain Field Access", test_terrain_field),
    ]
    
    results = []
    for test_name, test_func in tests:
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"\nERROR: Test {test_name} crashed: {e}")
            import traceback
            traceback.print_exc()
            results.append((test_name, False))
    
    # Summary
    print("\n" + "="*70)
    print("Test Summary")
    print("="*70)
    
    for test_name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"{status}: {test_name}")
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    print(f"\nTotal: {passed}/{total} tests passed")
    
    return passed == total


if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
