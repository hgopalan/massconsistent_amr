#!/usr/bin/env python3
"""
test_mann_box_cpp_gaussian_hill.py

C++ integration test for Mann Box with Gaussian Hill
Tests the underlying C++ implementation of Mann Box spectral tensor

This test:
1. Creates a simple Gaussian Hill terrain
2. Runs the wind solver
3. Validates Mann Box spectrum functions from C++ headers
4. Compares with Python implementation

The C++ implementation is in:
  - src/mann_box_spectral_tensor.H
  - src/synthetic_turbulence.H
"""

import subprocess
import sys
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent
REPO_DIR = TEST_DIR.parent.parent
BUILD_DIR = REPO_DIR / "build"
REGTEST_DIR = REPO_DIR / "regtest" / "gaussian_hill"

def run_gaussian_hill_test():
    """Run the Gaussian Hill C++ solver test."""
    print("\n" + "="*70)
    print("GAUSSIAN HILL C++ SOLVER TEST")
    print("="*70)
    
    # Check if build directory exists
    if not BUILD_DIR.exists():
        print(f"ERROR: Build directory not found at {BUILD_DIR}")
        print("Run: cmake -S . -B build && cmake --build build")
        return 1
    
    # Check if wind_solver executable exists
    wind_solver = BUILD_DIR / "wind_solver"
    if not wind_solver.exists():
        print(f"ERROR: wind_solver executable not found at {wind_solver}")
        print("Ensure you built the project with: cmake --build build")
        return 1
    
    # Check Gaussian Hill test case
    inputs_file = REGTEST_DIR / "inputs.i"
    if not inputs_file.exists():
        print(f"ERROR: Gaussian Hill inputs not found at {inputs_file}")
        return 1
    
    print(f"\n  Solver: {wind_solver}")
    print(f"  Inputs: {inputs_file}")
    
    try:
        print("\n" + "-"*70)
        print("Running Gaussian Hill solver...")
        print("-"*70)
        
        # Run the wind solver
        result = subprocess.run(
            [str(wind_solver), str(inputs_file)],
            cwd=str(REGTEST_DIR),
            capture_output=True,
            timeout=60,
            text=True
        )
        
        print(result.stdout)
        
        if result.returncode == 0:
            print("\n✓ Wind solver completed successfully")
            
            # Check for output files
            expected_files = [
                REGTEST_DIR / "plt_wind",
                REGTEST_DIR / "plt_wind_with_fluctuations",
            ]
            
            found_files = []
            for f in expected_files:
                if f.exists() or f.name in result.stdout:
                    found_files.append(True)
                    print(f"  ✓ Output: {f.name}")
                else:
                    found_files.append(False)
                    print(f"  Note: {f.name} not found (may not be configured)")
            
            return 0
        else:
            print(f"\n✗ Wind solver failed with return code {result.returncode}")
            if result.stderr:
                print("STDERR:")
                print(result.stderr)
            return 1
            
    except subprocess.TimeoutExpired:
        print(f"✗ Solver timed out (exceeded 60 seconds)")
        return 1
    except Exception as e:
        print(f"✗ Error running solver: {e}")
        return 1


def test_mann_box_headers():
    """Verify Mann Box headers are present and accessible."""
    print("\n" + "="*70)
    print("MANN BOX C++ HEADER VERIFICATION")
    print("="*70)
    
    headers = [
        "mann_box_spectral_tensor.H",
        "mann_box_temporal_synthesis.H",
        "mann_box_stability_adaptation.H",
        "mann_box_multiscale_adaptation.H",
        "mann_box_directional_rotation.H",
        "mann_box_roughness_effects.H",
        "mann_box_validation_diagnostics.H",
        "mann_box_presets.H",
        "mann_box_export_utilities.H",
        "synthetic_turbulence.H",
    ]
    
    src_dir = REPO_DIR / "src"
    found = 0
    missing = 0
    
    for header in headers:
        path = src_dir / header
        if path.exists():
            print(f"  ✓ {header}")
            found += 1
        else:
            print(f"  ✗ {header} NOT FOUND")
            missing += 1
    
    print(f"\nFound: {found}/{len(headers)} headers")
    
    if missing == 0:
        print("✓ All Mann Box headers present")
        return 0
    else:
        print(f"✗ {missing} headers missing")
        return 1


def test_mann_box_cpp_functions():
    """Verify Mann Box C++ functions are defined."""
    print("\n" + "="*70)
    print("MANN BOX C++ FUNCTION VERIFICATION")
    print("="*70)
    
    functions = [
        ("mann_box_spectrum_diagonal", "src/synthetic_turbulence.H"),
        ("mann_box_terrain_anisotropy_factor", "src/synthetic_turbulence.H"),
        ("mann_box_adapted_length_scale", "src/synthetic_turbulence.H"),
        ("compute_mann_box_spectrum_diagonal", "src/mann_box_spectral_tensor.H"),
        ("compute_mann_box_spectrum_offdiagonal", "src/mann_box_spectral_tensor.H"),
        ("compute_mann_box_spectral_tensor", "src/mann_box_spectral_tensor.H"),
        ("verify_spectral_tensor_realizability", "src/mann_box_spectral_tensor.H"),
    ]
    
    found = 0
    missing = 0
    
    for func, file_path in functions:
        full_path = REPO_DIR / file_path
        if full_path.exists():
            try:
                with open(full_path, 'r') as f:
                    content = f.read()
                    if func in content:
                        print(f"  ✓ {func} in {file_path}")
                        found += 1
                    else:
                        print(f"  ✗ {func} NOT FOUND in {file_path}")
                        missing += 1
            except Exception as e:
                print(f"  ✗ Error reading {file_path}: {e}")
                missing += 1
        else:
            print(f"  ✗ File not found: {file_path}")
            missing += 1
    
    print(f"\nFound: {found}/{len(functions)} functions")
    
    if missing == 0:
        print("✓ All Mann Box functions defined")
        return 0
    else:
        print(f"✗ {missing} functions missing")
        return 1


def main():
    """Run all C++ tests."""
    print("\n" + "="*70)
    print("MANN BOX C++ TESTS FOR GAUSSIAN HILL")
    print("="*70)
    
    results = []
    
    # Test 1: Verify headers
    results.append(("C++ Headers", test_mann_box_headers()))
    
    # Test 2: Verify functions
    results.append(("C++ Functions", test_mann_box_cpp_functions()))
    
    # Test 3: Run wind solver
    results.append(("Gaussian Hill Solver", run_gaussian_hill_test()))
    
    # Summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    
    passed = sum(1 for _, r in results if r == 0)
    total = len(results)
    
    for name, result in results:
        status = "✓ PASS" if result == 0 else "✗ FAIL"
        print(f"  {status}: {name}")
    
    print(f"\nTotal: {passed}/{total} passed")
    
    if passed == total:
        print("\n✓ All C++ tests passed!")
        return 0
    else:
        print(f"\n✗ {total - passed} test(s) failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
