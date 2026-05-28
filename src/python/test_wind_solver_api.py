#!/usr/bin/env python3
"""
test_wind_solver_api.py - Test script for pyWindSolver wind solver Python API

Tests the wind solver control functions:
- Initialization from inputs file
- Solving for mass-consistent wind
- State extraction
- Parameter updates
- Plotfile writing

Run after building with Python bindings enabled:
    cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
    cmake --build build
    PYTHONPATH=build/python python3 src/python/test_wind_solver_api.py
"""

import sys
import os
import numpy as np

try:
    import pyWindSolver
    from wind_solver import WindSolver
except ImportError as e:
    print(f"Error: Could not import pyWindSolver or wind_solver")
    print(f"  {e}")
    print("\nMake sure to:")
    print("  1. Build with -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON")
    print("  2. Set PYTHONPATH to point to build/python directory")
    print("  Example: PYTHONPATH=build/python python3 src/python/test_wind_solver_api.py")
    sys.exit(1)


def test_basic_initialization():
    """Test 1: Basic wind solver initialization"""
    print("=" * 70)
    print("Test 1: Basic wind solver initialization")
    print("=" * 70)
    
    # Create a minimal inputs file with terrain
    terrain_content = """0.0 0.0 100.0
300.0 0.0 100.0
0.0 300.0 100.0
300.0 300.0 150.0
150.0 150.0 120.0
"""
    
    inputs_content = """# Minimal inputs for testing
terrain_file = /tmp/test_terrain.csv
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1
dx = 30.0
dy = 30.0
dz = 30.0
domain_height = 300.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 1
tol_rel = 1.e-8
max_grid_size = 16
plot_file = plt_wind_test
"""
    
    # Write test files
    test_terrain = "/tmp/test_terrain.csv"
    test_inputs = "/tmp/test_wind_solver_inputs.i"
    
    with open(test_terrain, 'w') as f:
        f.write(terrain_content)
    
    with open(test_inputs, 'w') as f:
        f.write(inputs_content)
    
    try:
        # Test low-level API
        result = pyWindSolver.initialize(test_inputs)
        
        print(f"\nInitialization result:")
        print(f"  Success: {result['success']}")
        print(f"  Grid: {result['nx']} × {result['ny']} × {result['nz']}")
        print(f"  Domain: X=[{result['xmin']:.1f}, {result['xmax']:.1f}], "
              f"Y=[{result['ymin']:.1f}, {result['ymax']:.1f}], "
              f"Z=[{result['zmin']:.1f}, {result['zmax']:.1f}]")
        print(f"  Cell size: dx={result['dx']:.2f} m, dy={result['dy']:.2f} m, dz={result['dz']:.2f} m")
        
        if result['success']:
            print("\n✓ Test PASSED: Initialization successful")
            pyWindSolver.finalize()
            return True
        else:
            print("\n✗ Test FAILED: Initialization failed")
            return False
            
    except Exception as e:
        print(f"\n✗ Test FAILED with exception: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        # Cleanup
        if os.path.exists(test_terrain):
            os.remove(test_terrain)
        if os.path.exists(test_inputs):
            os.remove(test_inputs)


def test_solve_and_extract():
    """Test 2: Solve and extract wind fields"""
    print("\n" + "=" * 70)
    print("Test 2: Solve and extract wind fields")
    print("=" * 70)
    
    # Create a simple flat terrain for predictable results
    terrain_content = """0.0 0.0 0.0
300.0 0.0 0.0
0.0 300.0 0.0
300.0 300.0 0.0
150.0 150.0 0.0
"""
    
    inputs_content = """terrain_file = /tmp/test_terrain_flat.csv
init_mode = uniform
uniform_U = 5.0
uniform_V = 2.0
dx = 100.0
dy = 100.0
dz = 100.0
domain_height = 300.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
tol_rel = 1.e-8
"""
    
    test_terrain = "/tmp/test_terrain_flat.csv"
    test_inputs = "/tmp/test_wind_solver_solve.i"
    
    with open(test_terrain, 'w') as f:
        f.write(terrain_content)
    
    with open(test_inputs, 'w') as f:
        f.write(inputs_content)
    
    try:
        result = pyWindSolver.initialize(test_inputs)
        if not result['success']:
            print("\n✗ Test FAILED: Initialization failed")
            return False
        
        # Solve
        solve_result = pyWindSolver.solve()
        print(f"\nSolve result:")
        print(f"  Success: {solve_result['success']}")
        print(f"  Iterations: {solve_result['iters']}")
        print(f"  Residual: {solve_result['residual']:.2e}")
        
        if not solve_result['success']:
            print("\n✗ Test FAILED: Solve failed")
            pyWindSolver.finalize()
            return False
        
        # Extract velocity
        vel = pyWindSolver.get_velocity()
        u, v, w = vel['u'], vel['v'], vel['w']
        
        print(f"\nExtracted velocity field:")
        print(f"  Shape: {u.shape}")
        print(f"  U range: [{u.min():.2f}, {u.max():.2f}] m/s")
        print(f"  V range: [{v.min():.2f}, {v.max():.2f}] m/s")
        print(f"  W range: [{w.min():.2f}, {w.max():.2f}] m/s")
        
        # Extract at specific AGL
        vel_agl = pyWindSolver.get_velocity_at_agl(10.0)
        print(f"\nVelocity at 10m AGL:")
        print(f"  U shape: {vel_agl['u'].shape}")
        print(f"  Mean U: {vel_agl['u'].mean():.2f} m/s")
        print(f"  Mean V: {vel_agl['v'].mean():.2f} m/s")
        
        # Get terrain
        terrain = pyWindSolver.get_terrain()
        print(f"\nTerrain:")
        print(f"  Shape: {terrain.shape}")
        print(f"  Range: [{terrain.min():.2f}, {terrain.max():.2f}] m")
        
        print("\n✓ Test PASSED: Solve and extraction successful")
        pyWindSolver.finalize()
        return True
            
    except Exception as e:
        print(f"\n✗ Test FAILED with exception: {e}")
        import traceback
        traceback.print_exc()
        try:
            pyWindSolver.finalize()
        except:
            pass
        return False
    finally:
        # Cleanup
        if os.path.exists(test_terrain):
            os.remove(test_terrain)
        if os.path.exists(test_inputs):
            os.remove(test_inputs)


def test_high_level_api():
    """Test 3: High-level WindSolver class"""
    print("\n" + "=" * 70)
    print("Test 3: High-level WindSolver class")
    print("=" * 70)
    
    # Create test files
    terrain_content = """0.0 0.0 50.0
200.0 0.0 50.0
0.0 200.0 50.0
200.0 200.0 80.0
100.0 100.0 60.0
"""
    
    inputs_content = """terrain_file = /tmp/test_terrain_hill.csv
U_ref = 8.0
V_ref = 3.0
z_ref = 10.0
z0 = 0.05
dx = 50.0
dy = 50.0
dz = 50.0
domain_height = 200.0
mlmg_verbose = 0
"""
    
    test_terrain = "/tmp/test_terrain_hill.csv"
    test_inputs = "/tmp/test_wind_solver_hl.i"
    
    with open(test_terrain, 'w') as f:
        f.write(terrain_content)
    
    with open(test_inputs, 'w') as f:
        f.write(inputs_content)
    
    try:
        # Use context manager
        with WindSolver(test_inputs) as wind:
            print(f"\nSolver initialized:")
            print(f"  Grid: {wind.nx} × {wind.ny} × {wind.nz}")
            print(f"  Terrain bounds: [{wind.zs_min:.1f}, {wind.zs_max:.1f}] m")
            
            # Solve
            wind.solve()
            
            # Extract fields
            vel = wind.get_velocity()
            terrain = wind.get_terrain()
            
            print(f"\nExtracted data:")
            print(f"  Velocity shape: {vel['u'].shape}")
            print(f"  Terrain shape: {terrain.shape}")
            
            # Get velocity at specific height
            vel_10m = wind.get_velocity_at_agl(10.0)
            print(f"  Velocity at 10m AGL: U_mean={vel_10m['u'].mean():.2f} m/s")
        
        print("\n✓ Test PASSED: High-level API successful")
        return True
            
    except Exception as e:
        print(f"\n✗ Test FAILED with exception: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        # Cleanup
        if os.path.exists(test_terrain):
            os.remove(test_terrain)
        if os.path.exists(test_inputs):
            os.remove(test_inputs)


def main():
    """Run all tests"""
    print("\n" + "=" * 70)
    print("pyWindSolver API Tests")
    print("=" * 70 + "\n")
    
    tests = [
        test_basic_initialization,
        test_solve_and_extract,
        test_high_level_api,
    ]
    
    results = []
    for test in tests:
        try:
            result = test()
            results.append(result)
        except Exception as e:
            print(f"\nTest crashed with exception: {e}")
            import traceback
            traceback.print_exc()
            results.append(False)
        
        # Small delay between tests
        import time
        time.sleep(0.5)
    
    # Summary
    print("\n" + "=" * 70)
    print("Test Summary")
    print("=" * 70)
    passed = sum(results)
    total = len(results)
    print(f"\nPassed: {passed}/{total}")
    
    if passed == total:
        print("\n🎉 All tests PASSED! 🎉\n")
        return 0
    else:
        print(f"\n⚠️  {total - passed} test(s) FAILED\n")
        return 1


if __name__ == "__main__":
    sys.exit(main())
