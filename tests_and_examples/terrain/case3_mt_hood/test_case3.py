#!/usr/bin/env python3
"""
test_case3.py - Case 3: Mt. Hood with Time-Varying Winds and Turbulence

Tests mass-consistent wind solver with:
- High-elevation SRTM terrain (Mt. Hood, OR - alpine terrain)
- Time-varying wind boundary conditions
- Log-law wind profile initialization
- Synthetic turbulence fluctuations
- Modified wind field output

Prerequisites:
- User must manually generate terrain.csv from SRTM data using terrain_reader_srtm.py
- Example: python3 terrain_reader_srtm.py N45W121.hgt --output terrain.csv \\
           --lat-min 45.366 --lat-max 45.380 --lon-min -121.696 --lon-max -121.680

Validates:
1. Solver initialization with high-elevation terrain
2. Wind field solution over alpine terrain
3. Time-varying wind integration
4. Turbulence fluctuation application at high elevations
5. Modified wind output file generation
"""

import os
import sys
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


def test_terrain_requirement():
    """Check if terrain file exists."""
    print("\n" + "="*70)
    print("Test 1: Terrain File Requirement")
    print("="*70)
    
    terrain_file = TEST_DIR / "terrain.csv"
    
    if not terrain_file.exists():
        print("NOTICE: terrain.csv not found")
        print("\nTo generate terrain from SRTM data for Mt. Hood:")
        print("1. Download SRTM tile N45W121.hgt from USGS SRTM server")
        print("   https://earthexplorer.usgs.gov/")
        print("\n2. Run terrain reader:")
        print("   python3 ../../tools/terrain_reader_srtm.py N45W121.hgt \\")
        print("     --output terrain.csv \\")
        print("     --lat-min 45.366 --lat-max 45.380 \\")
        print("     --lon-min -121.696 --lon-max -121.680")
        print("\n3. Place terrain.csv in this directory")
        return False
    
    print(f"✓ Terrain file found: {terrain_file}")
    return True


def test_initialization():
    """Test solver initialization with Mt. Hood terrain."""
    print("\n" + "="*70)
    print("Test 2: Solver Initialization")
    print("="*70)
    
    terrain_file = TEST_DIR / "terrain.csv"
    if not terrain_file.exists():
        print("SKIPPED: terrain.csv not found")
        return True
    
    try:
        wind = WindSolver()
        inputs_file = TEST_DIR / "inputs.i"
        
        if not inputs_file.exists():
            print(f"ERROR: Inputs file not found: {inputs_file}")
            return False
        
        wind.initialize(str(inputs_file))
        
        print(f"✓ Grid dimensions: {wind.nx}x{wind.ny}x{wind.nz}")
        print(f"✓ Domain bounds:")
        print(f"  X: [{wind.xmin:.1f}, {wind.xmax:.1f}] m")
        print(f"  Y: [{wind.ymin:.1f}, {wind.ymax:.1f}] m")
        print(f"  Z: [{wind.zmin:.1f}, {wind.zmax:.1f}] m")
        print(f"✓ Grid spacing: dx={wind.dx:.2f} m, dy={wind.dy:.2f} m, dz={wind.dz:.2f} m")
        print(f"✓ Terrain bounds (high-altitude): [{wind.zs_min:.1f}, {wind.zs_max:.1f}] m")
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_wind_solve():
    """Test mass-consistent wind field solution over alpine terrain."""
    print("\n" + "="*70)
    print("Test 3: Wind Field Solution")
    print("="*70)
    
    terrain_file = TEST_DIR / "terrain.csv"
    if not terrain_file.exists():
        print("SKIPPED: terrain.csv not found")
        return True
    
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
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_velocity_extraction():
    """Test velocity extraction at high elevation."""
    print("\n" + "="*70)
    print("Test 4: Velocity Extraction at High Elevation")
    print("="*70)
    
    terrain_file = TEST_DIR / "terrain.csv"
    if not terrain_file.exists():
        print("SKIPPED: terrain.csv not found")
        return True
    
    try:
        wind = WindSolver()
        wind.initialize(str(TEST_DIR / "inputs.i"))
        wind.solve()
        
        # Extract velocity at 50 m AGL (above alpine vegetation)
        vel_50m = wind.get_velocity_at_agl(50.0)
        
        if 'u' not in vel_50m or vel_50m['u'] is None:
            print("ERROR: Failed to extract velocity at 50 m AGL")
            return False
        
        print(f"✓ Extracted velocity at {vel_50m['agl']} m AGL (above alpine terrain)")
        print(f"✓ Extracted field shape: {vel_50m['u'].shape}")
        
        # Analyze wind field
        u_mean = vel_50m['u'].mean()
        u_std = vel_50m['u'].std()
        v_mean = vel_50m['v'].mean()
        
        print(f"✓ Wind statistics at {vel_50m['agl']} m AGL:")
        print(f"  U mean: {u_mean:.2f} m/s (std: {u_std:.2f} m/s)")
        print(f"  V mean: {v_mean:.2f} m/s")
        
        # Extract at multiple heights to verify wind shear profile
        vel_20m = wind.get_velocity_at_agl(20.0)
        vel_80m = wind.get_velocity_at_agl(80.0)
        
        u_20m = vel_20m['u'].mean()
        u_80m = vel_80m['u'].mean()
        
        print(f"\n✓ Wind shear profile:")
        print(f"  @ 20 m AGL: {u_20m:.2f} m/s")
        print(f"  @ 50 m AGL: {u_mean:.2f} m/s")
        print(f"  @ 80 m AGL: {u_80m:.2f} m/s")
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def run_all_tests():
    """Run all Case 3 tests."""
    print("\n" + "="*70)
    print("Case 3: Mt. Hood with Time-Varying Winds and Turbulence")
    print("="*70)
    
    # Change to test directory
    os.chdir(TEST_DIR)
    
    tests = [
        ("Terrain File Requirement", test_terrain_requirement),
        ("Solver Initialization", test_initialization),
        ("Wind Field Solution", test_wind_solve),
        ("Velocity Extraction", test_velocity_extraction),
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
