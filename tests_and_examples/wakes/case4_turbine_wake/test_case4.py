#!/usr/bin/env python3
"""
test_case4.py - Case 4: Flat Terrain with Wind Turbines and Analytical Wake Model

Tests mass-consistent wind solver with:
- Flat terrain
- Wind turbine wake modeling (Jensen model)
- Wake velocity deficit calculation
- Wind turbine power output and inflow wind speed retrieval
- Automatic logging of turbine power outputs to CSV

Validates:
1. Solver initialization and grid parameters with turbines
2. Wind field solution convergence with analytical wake model enabled
3. Inflow wind speed extraction and wake deficit validation
4. Turbine power output extraction
5. Power output CSV file generation
"""

import os
import sys
import math
from pathlib import Path

# Add parent directory to path for wind_solver import
TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent.parent.parent / "src" / "python"
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    print("Make sure to build with Python bindings enabled:")
    print("  cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON")
    sys.exit(1)


def test_initialization():
    """Test solver initialization with turbine parameters."""
    print("\n" + "="*70)
    print("Test 1: Solver Initialization with Turbines")
    print("="*70)
    
    try:
        wind = WindSolver()
        inputs_file = TEST_DIR / "inputs.i"
        
        if not inputs_file.exists():
            print(f"ERROR: Inputs file not found: {inputs_file}")
            return False
        
        wind.initialize(str(inputs_file))
        
        # Verify grid parameters
        expected_nx, expected_ny, expected_nz = 10, 10, 10
        
        if wind.nx != expected_nx or wind.ny != expected_ny or wind.nz != expected_nz:
            print(f"ERROR: Grid dimensions mismatch:")
            print(f"  Expected: {expected_nx}x{expected_ny}x{expected_nz}")
            print(f"  Got: {wind.nx}x{wind.ny}x{wind.nz}")
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


def test_wind_solve_and_wake():
    """Test wind field solution with analytical wake model and verify deficits."""
    print("\n" + "="*70)
    print("Test 2: Wind Solver and Turbine Wake Deficit Verification")
    print("="*70)
    
    try:
        wind = WindSolver()
        inputs_file = TEST_DIR / "inputs.i"
        wind.initialize(str(inputs_file))
        
        # Solve wind field
        print("Solving wind field...")
        result = wind.solve()
        
        if not result or not result.get('success', False):
            print("ERROR: Wind solve failed")
            return False
            
        print("✓ Wind solve completed successfully")
        
        # Retrieve power and inflow speed results
        power_outputs = wind.get_turbine_power_outputs()
        inflow_speeds = wind.get_turbine_inflow_speeds()
        
        print(f"✓ Turbine 0 (Upstream at x=20): Inflow speed = {inflow_speeds[0]:.2f} m/s, Power = {power_outputs[0]:.2f} kW")
        print(f"✓ Turbine 1 (Downstream at x=80): Inflow speed = {inflow_speeds[1]:.2f} m/s, Power = {power_outputs[1]:.2f} kW")
        
        # Verify that velocity deficit is experienced
        if inflow_speeds[0] <= 0.1:
            print("ERROR: Upstream turbine got zero or near-zero wind!")
            return False
            
        if inflow_speeds[1] >= inflow_speeds[0]:
            print("ERROR: Downstream turbine did not experience velocity deficit!")
            return False
            
        print("✓ Success: Downstream turbine experienced wake velocity deficit!")
        
        # Verify logging CSV has been generated
        log_csv = TEST_DIR / "turbine_power_output.csv"
        if not log_csv.exists():
            print(f"ERROR: {log_csv} was not generated!")
            return False
            
        print("✓ Success: Log CSV file generated!")
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def run_all_tests():
    """Run all Case 4 tests."""
    print("\n" + "="*70)
    print("Case 4: Flat Terrain with Wind Turbines and Analytical Wake Model")
    print("="*70)
    
    # Change to test directory
    os.chdir(TEST_DIR)
    
    tests = [
        ("Solver Initialization", test_initialization),
        ("Wind Solver and Wake Deficits", test_wind_solve_and_wake),
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
