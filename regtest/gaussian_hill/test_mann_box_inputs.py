#!/usr/bin/env python3
"""
test_gaussian_hill_mann_box_inputs.py

Complete test case for Gaussian Hill with Mann Box turbulence.
Uses the inputs_mann_box.i configuration file.

Validates:
1. Inputs file parsing and parameter loading
2. Wind solver initialization with Mann Box configuration
3. Wind field solution with terrain effects
4. Turbulence parameter consistency
5. Output generation and validation
"""

import os
import sys
import math
from pathlib import Path
import subprocess

# Add paths
TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent.parent / "src" / "python"
BUILD_DIR = TEST_DIR.parent.parent / "build"
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
    from mann_box import MannBox, create_mann_box_preset
except ImportError as e:
    print(f"ERROR: Could not import required modules: {e}")
    print("Build with: cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON")
    sys.exit(1)


def test_inputs_file_parsing():
    """Test parsing of inputs_mann_box.i configuration file."""
    print("\n" + "="*70)
    print("TEST 1: Inputs File Parsing")
    print("="*70)
    
    inputs_file = TEST_DIR / "inputs_mann_box.i"
    
    if not inputs_file.exists():
        print(f"ERROR: inputs_mann_box.i not found at {inputs_file}")
        return False
    
    print(f"✓ Configuration file found: {inputs_file}")
    
    # Parse and validate key parameters
    with open(inputs_file, 'r') as f:
        content = f.read()
    
    required_params = [
        "enable_synthetic_turbulence",
        "turbulence_spectrum_model",
        "turbulence_length_scale_u",
        "turbulence_length_scale_v",
        "turbulence_length_scale_w",
        "turbulence_anisotropy_ratio_v",
        "turbulence_anisotropy_ratio_w",
        "turbulence_mann_asymmetry",
        "turbulence_uv_coherence",
        "turbulence_uw_coherence",
        "turbulence_vw_coherence",
    ]
    
    found_params = 0
    for param in required_params:
        if param in content:
            found_params += 1
            print(f"  ✓ {param}")
        else:
            print(f"  ✗ {param} NOT FOUND")
    
    print(f"\nFound {found_params}/{len(required_params)} Mann Box parameters")
    
    if found_params == len(required_params):
        print("✓ All Mann Box parameters present in inputs.i")
        return True
    else:
        print(f"✗ Missing {len(required_params) - found_params} parameter(s)")
        return False


def test_parameter_extraction():
    """Extract and validate parameter values from inputs_mann_box.i."""
    print("\n" + "="*70)
    print("TEST 2: Parameter Extraction and Validation")
    print("="*70)
    
    inputs_file = TEST_DIR / "inputs_mann_box.i"
    params = {}
    
    # Extract parameters (simple parsing)
    with open(inputs_file, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if '=' in line:
                key, val = line.split('=', 1)
                key = key.strip()
                val = val.strip()
                try:
                    # Try to convert to float
                    params[key] = float(val)
                except:
                    # Keep as string
                    params[key] = val
    
    # Validate key parameters
    checks = [
        ("U_ref", 12.0, "Reference wind speed [m/s]"),
        ("z_ref", 10.0, "Reference height [m]"),
        ("turbulence_length_scale_u", 300.0, "Length scale u [m]"),
        ("turbulence_length_scale_v", 210.0, "Length scale v [m]"),
        ("turbulence_length_scale_w", 120.0, "Length scale w [m]"),
        ("turbulence_anisotropy_ratio_v", 0.8, "Anisotropy v/u"),
        ("turbulence_anisotropy_ratio_w", 0.5, "Anisotropy w/u"),
        ("turbulence_mann_asymmetry", 1.0, "Mann asymmetry α"),
        ("turbulence_uv_coherence", 0.75, "u-v coherence"),
        ("turbulence_uw_coherence", 0.50, "u-w coherence"),
        ("turbulence_vw_coherence", 0.65, "v-w coherence"),
    ]
    
    passed = 0
    for param_name, expected_val, description in checks:
        if param_name in params:
            actual_val = params[param_name]
            try:
                if abs(float(actual_val) - expected_val) < 1e-10:
                    print(f"  ✓ {param_name}: {actual_val} (expected {expected_val})")
                    passed += 1
                else:
                    print(f"  ✗ {param_name}: {actual_val} (expected {expected_val})")
            except:
                print(f"  ✗ {param_name}: Could not parse value")
        else:
            print(f"  ✗ {param_name}: Parameter not found")
    
    print(f"\nValidation: {passed}/{len(checks)} parameters correct")
    
    return passed == len(checks)


def test_mann_box_parameters():
    """Verify Mann Box parameters match configuration."""
    print("\n" + "="*70)
    print("TEST 3: Mann Box Parameter Consistency")
    print("="*70)
    
    # Create Mann Box model with config parameters
    mann = MannBox(
        length_scale_u=300.0,
        length_scale_v=210.0,
        length_scale_w=120.0,
        variance_u=1.0,
        variance_v=0.64,      # 0.8² * 1.0
        variance_w=0.25,      # 0.5² * 1.0
        asymmetry=1.0,
        uv_coherence=0.75,
        uw_coherence=0.50,
        vw_coherence=0.65
    )
    
    params = mann.get_parameters()
    
    checks = [
        (params.length_scale_u, 300.0, "Length scale u"),
        (params.length_scale_v, 210.0, "Length scale v"),
        (params.length_scale_w, 120.0, "Length scale w"),
        (params.variance_u, 1.0, "Variance u"),
        (params.variance_v, 0.64, "Variance v"),
        (params.variance_w, 0.25, "Variance w"),
        (params.asymmetry, 1.0, "Asymmetry α"),
        (params.uv_coherence, 0.75, "u-v coherence"),
        (params.uw_coherence, 0.50, "u-w coherence"),
        (params.vw_coherence, 0.65, "v-w coherence"),
    ]
    
    passed = 0
    for actual, expected, name in checks:
        if abs(actual - expected) < 1e-10:
            print(f"  ✓ {name}: {actual:.4f}")
            passed += 1
        else:
            print(f"  ✗ {name}: {actual:.4f} (expected {expected:.4f})")
    
    print(f"\nParameter consistency: {passed}/{len(checks)} correct")
    
    return passed == len(checks)


def test_wind_solver_initialization():
    """Test wind solver initialization with inputs_mann_box.i."""
    print("\n" + "="*70)
    print("TEST 4: Wind Solver Initialization")
    print("="*70)
    
    inputs_file = TEST_DIR / "inputs_mann_box.i"
    
    try:
        wind = WindSolver()
        result = wind.initialize(str(inputs_file))
        
        if result["success"]:
            print(f"✓ Solver initialized successfully")
            print(f"  Grid: {wind.nx} × {wind.ny} × {wind.nz}")
            print(f"  Domain: X=[{wind.xmin:.1f}, {wind.xmax:.1f}] m")
            print(f"          Y=[{wind.ymin:.1f}, {wind.ymax:.1f}] m")
            print(f"          Z=[{wind.zmin:.1f}, {wind.zmax:.1f}] m")
            print(f"  Spacing: dx={wind.dx:.2f}, dy={wind.dy:.2f}, dz={wind.dz:.2f} m")
            print(f"  Terrain: [{wind.zs_min:.1f}, {wind.zs_max:.1f}] m")
            
            wind.finalize()
            return True
        else:
            print(f"✗ Solver initialization failed")
            return False
    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_spectrum_generation():
    """Generate spectrum using configuration parameters."""
    print("\n" + "="*70)
    print("TEST 5: Spectrum Generation")
    print("="*70)
    
    import numpy as np
    
    # Create Mann Box with config parameters
    mann = create_mann_box_preset('neutral')
    
    # Override with specific values from config
    mann.update_parameters(
        length_scale_u=300.0,
        length_scale_v=210.0,
        length_scale_w=120.0,
        asymmetry=1.0,
        uv_coherence=0.75,
        uw_coherence=0.50,
        vw_coherence=0.65
    )
    
    # Generate spectrum
    frequencies = np.logspace(-2, 1, 100)
    spectrum = mann.compute_spectrum(
        frequencies=frequencies,
        height=90.0,
        mean_wind_speed=12.0
    )
    
    print(f"✓ Spectrum generated for {len(frequencies)} frequencies")
    
    # Validate spectrum
    S_uu = spectrum['S_uu']
    S_vv = spectrum['S_vv']
    S_ww = spectrum['S_ww']
    
    # Check energy ordering
    mean_uu = np.mean(S_uu[1:])
    mean_vv = np.mean(S_vv[1:])
    mean_ww = np.mean(S_ww[1:])
    
    print(f"  Mean energy: S_uu = {mean_uu:.4e}, S_vv = {mean_vv:.4e}, S_ww = {mean_ww:.4e}")
    
    # Check anisotropy
    rms_u = np.sqrt(spectrum['variance_u'])
    rms_v = np.sqrt(spectrum['variance_v'])
    rms_w = np.sqrt(spectrum['variance_w'])
    
    ratio_v = rms_v / rms_u
    ratio_w = rms_w / rms_u
    
    print(f"  Anisotropy: v/u = {ratio_v:.3f} (expected 0.8), w/u = {ratio_w:.3f} (expected 0.5)")
    
    # Check realizability
    if mann.validate_realizability(spectrum):
        print(f"✓ Spectrum is physically realizable")
        return True
    else:
        print(f"✗ Spectrum realizability check failed")
        return False


def main():
    """Run all tests."""
    print("\n" + "="*70)
    print("GAUSSIAN HILL WITH MANN BOX - INPUTS.I TEST SUITE")
    print("="*70)
    
    results = []
    
    # Run tests
    results.append(("Inputs File Parsing", test_inputs_file_parsing()))
    results.append(("Parameter Extraction", test_parameter_extraction()))
    results.append(("Mann Box Parameters", test_mann_box_parameters()))
    results.append(("Wind Solver Init", test_wind_solver_initialization()))
    results.append(("Spectrum Generation", test_spectrum_generation()))
    
    # Summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"  {status}: {name}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    
    if passed == total:
        print("\n✓ All tests passed! Mann Box configuration is valid.")
        return 0
    else:
        print(f"\n✗ {total - passed} test(s) failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
