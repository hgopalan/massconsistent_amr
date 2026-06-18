#!/usr/bin/env python3
"""
test_mann_box_gaussian_hill_integration.py

Integration test: Gaussian Hill wind solver + Mann Box turbulence generation

This test validates the complete workflow:
1. Initialize wind solver with Gaussian Hill terrain
2. Generate Mann Box spectra
3. Validate spectrum realizability
4. Prepare for integration with turbulence synthesis

Requires:
  - Built wind solver with Python bindings
  - Gaussian Hill test case (regtest/terrain/gaussian_hill/)
"""

import os
import sys
import json
from pathlib import Path
import numpy as np

# Add paths
TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent.parent / "src" / "python"
REGTEST_DIR = TEST_DIR.parent.parent / "regtest" / "terrain" / "gaussian_hill"
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
    from mann_box import MannBox, create_mann_box_preset
except ImportError as e:
    print(f"ERROR: Import failed: {e}")
    print("Build with: cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON")
    sys.exit(1)


def test_gaussian_hill_with_mann_box():
    """Test Mann Box integration with Gaussian Hill solver."""
    print("\n" + "="*70)
    print("GAUSSIAN HILL + MANN BOX INTEGRATION TEST")
    print("="*70)
    
    passed = 0
    failed = 0
    
    # Check regtest directory
    if not REGTEST_DIR.exists():
        print(f"ERROR: Gaussian Hill test case not found at {REGTEST_DIR}")
        print("Expected directory structure: regtest/terrain/gaussian_hill/")
        return 1
    
    inputs_file = REGTEST_DIR / "inputs.i"
    if not inputs_file.exists():
        print(f"ERROR: inputs.i not found at {inputs_file}")
        return 1
    
    print("\n" + "-"*70)
    print("Wind Solver Initialization")
    print("-"*70)
    
    try:
        wind = WindSolver()
        result = wind.initialize(str(inputs_file))
        
        if result["success"]:
            print(f"✓ Solver initialized")
            print(f"  Grid: {wind.nx} × {wind.ny} × {wind.nz}")
            print(f"  Domain: X=[{wind.xmin:.1f}, {wind.xmax:.1f}] m")
            print(f"           Y=[{wind.ymin:.1f}, {wind.ymax:.1f}] m")
            print(f"           Z=[{wind.zmin:.1f}, {wind.zmax:.1f}] m")
            print(f"  Spacing: dx={wind.dx:.2f}, dy={wind.dy:.2f}, dz={wind.dz:.2f} m")
            print(f"  Terrain: Z_s ∈ [{wind.zs_min:.1f}, {wind.zs_max:.1f}] m")
            passed += 1
        else:
            print(f"✗ Solver initialization failed")
            failed += 1
            wind.finalize()
            return 1
    except Exception as e:
        print(f"✗ Solver initialization error: {e}")
        failed += 1
        return 1
    
    print("\n" + "-"*70)
    print("Mass-Consistent Wind Field Solution")
    print("-"*70)
    
    try:
        solve_result = wind.solve()
        
        if solve_result["success"] and solve_result["solved"]:
            print(f"✓ Wind field solved")
            print(f"  Iterations: {solve_result['iters']}")
            print(f"  Residual: {solve_result['residual']:.2e}")
            passed += 1
        else:
            print(f"✗ Wind solve failed or did not converge")
            failed += 1
            wind.finalize()
            return 1
    except Exception as e:
        print(f"✗ Wind solve error: {e}")
        failed += 1
        wind.finalize()
        return 1
    
    print("\n" + "-"*70)
    print("Mann Box Spectrum Generation")
    print("-"*70)
    
    try:
        # Create Mann Box model
        mann = create_mann_box_preset('neutral')
        
        # Generate spectra at multiple frequencies
        frequencies = np.logspace(-2, 1, 50)  # 0.01 to 10 Hz
        spectrum = mann.compute_spectrum(
            frequencies=frequencies,
            height=90.0,
            mean_wind_speed=12.0
        )
        
        print(f"✓ Mann Box spectrum computed")
        print(f"  Frequencies: {len(frequencies)} points from {frequencies[0]:.4f} to {frequencies[-1]:.4f} Hz")
        print(f"  Mean S_uu: {np.mean(spectrum['S_uu'][1:]):.4e} m³/s²")
        print(f"  Mean S_vv: {np.mean(spectrum['S_vv'][1:]):.4e} m³/s²")
        print(f"  Mean S_ww: {np.mean(spectrum['S_ww'][1:]):.4e} m³/s²")
        
        # Compute RMS values from spectral integration
        df = frequencies[1] - frequencies[0]  # Approximate frequency resolution
        rms_u = np.sqrt(2.0 * np.trapz(spectrum['S_uu'], frequencies))
        rms_v = np.sqrt(2.0 * np.trapz(spectrum['S_vv'], frequencies))
        rms_w = np.sqrt(2.0 * np.trapz(spectrum['S_ww'], frequencies))
        
        print(f"  Integrated RMS:")
        print(f"    u': {rms_u:.4f} m/s")
        print(f"    v': {rms_v:.4f} m/s")
        print(f"    w': {rms_w:.4f} m/s")
        
        passed += 1
    except Exception as e:
        print(f"✗ Mann Box spectrum generation error: {e}")
        import traceback
        traceback.print_exc()
        failed += 1
    
    print("\n" + "-"*70)
    print("Spectrum Realizability Validation")
    print("-"*70)
    
    try:
        if mann.validate_realizability(spectrum):
            print(f"✓ Spectrum realizability verified")
            
            # Additional checks
            S_uu = spectrum['S_uu']
            S_vv = spectrum['S_vv']
            S_ww = spectrum['S_ww']
            
            # Check energy ordering
            mean_uu = np.mean(S_uu[1:])
            mean_vv = np.mean(S_vv[1:])
            mean_ww = np.mean(S_ww[1:])
            
            print(f"  Energy ordering: S_uu > S_vv > S_ww")
            print(f"    {mean_uu:.4e} > {mean_vv:.4e} > {mean_ww:.4e}")
            
            if mean_uu >= mean_vv >= mean_ww:
                print(f"  ✓ Correct ordering confirmed")
                passed += 1
            else:
                print(f"  ✗ Energy ordering incorrect")
                failed += 1
        else:
            print(f"✗ Spectrum realizability check failed")
            failed += 1
    except Exception as e:
        print(f"✗ Realizability validation error: {e}")
        failed += 1
    
    print("\n" + "-"*70)
    print("Spectrum Parameter Summary")
    print("-"*70)
    
    try:
        params = mann.get_parameters()
        print(f"  Length Scales:")
        print(f"    L_u = {params.length_scale_u:.1f} m")
        print(f"    L_v = {params.length_scale_v:.1f} m")
        print(f"    L_w = {params.length_scale_w:.1f} m")
        print(f"  Variances:")
        print(f"    σ²_u = {params.variance_u:.4f} m²/s²")
        print(f"    σ²_v = {params.variance_v:.4f} m²/s²")
        print(f"    σ²_w = {params.variance_w:.4f} m²/s²")
        print(f"  Coherence Factors:")
        print(f"    η_uv = {params.uv_coherence:.2f}")
        print(f"    η_uw = {params.uw_coherence:.2f}")
        print(f"    η_vw = {params.vw_coherence:.2f}")
        print(f"  Asymmetry: α = {params.asymmetry:.2f}")
        passed += 1
    except Exception as e:
        print(f"✗ Parameter summary error: {e}")
        failed += 1
    
    print("\n" + "-"*70)
    print("Multiple Preset Validation")
    print("-"*70)
    
    try:
        presets = ['neutral', 'stable', 'unstable', 'wind_farm', 'complex_terrain']
        preset_results = []
        
        for preset in presets:
            try:
                m = create_mann_box_preset(preset)
                s = m.compute_spectrum(frequencies=frequencies, height=90.0, mean_wind_speed=12.0)
                if m.validate_realizability(s):
                    print(f"  ✓ Preset '{preset}' is valid")
                    preset_results.append(True)
                else:
                    print(f"  ✗ Preset '{preset}' failed realizability check")
                    preset_results.append(False)
            except Exception as e:
                print(f"  ✗ Preset '{preset}' error: {e}")
                preset_results.append(False)
        
        if all(preset_results):
            print(f"\n✓ All {len(presets)} presets validated successfully")
            passed += 1
        else:
            failures = len([p for p in preset_results if not p])
            print(f"\n✗ {failures}/{len(presets)} presets failed")
            failed += 1
    except Exception as e:
        print(f"✗ Preset validation error: {e}")
        failed += 1
    
    # Cleanup
    try:
        wind.finalize()
    except:
        pass
    
    # Summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    total = passed + failed
    print(f"Total: {total} checks")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    
    if failed == 0:
        print("\n✓ All integration tests passed!")
        return 0
    else:
        print(f"\n✗ {failed} check(s) failed")
        return 1


if __name__ == "__main__":
    sys.exit(test_gaussian_hill_with_mann_box())
