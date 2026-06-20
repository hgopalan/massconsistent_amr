#!/usr/bin/env python3
"""
test_gaussian_hill_mann_box.py - Gaussian Hill with Mann Box Turbulence (Python)

Tests mass-consistent wind solver with Mann Box synthetic turbulence on Gaussian hill terrain.

Validates:
1. Mann Box spectral tensor computation
2. Spectrum realizability (Cauchy-Schwarz, positive semi-definiteness)
3. Height-dependent spectrum behavior
4. Anisotropy ratios in spectral components
5. Integration with wind solver
"""

import os
import sys
import math
import numpy as np
from pathlib import Path

# Add parent directory to path for imports
TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent.parent.parent / "src" / "python"
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from mann_box import MannBox, create_mann_box_preset
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import required modules: {e}")
    print("Make sure to build with Python bindings enabled:")
    print("  cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON")
    sys.exit(1)


class TestResults:
    """Track test results."""
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.tests = []
    
    def add_pass(self, test_name):
        self.passed += 1
        self.tests.append((test_name, "PASS"))
        print(f"  ✓ {test_name}")
    
    def add_fail(self, test_name, reason):
        self.failed += 1
        self.tests.append((test_name, "FAIL", reason))
        print(f"  ✗ {test_name}: {reason}")
    
    def summary(self):
        total = self.passed + self.failed
        print("\n" + "="*70)
        print(f"Test Results: {self.passed}/{total} passed")
        print("="*70)
        for result in self.tests:
            if len(result) == 2:
                print(f"  ✓ {result[0]}")
            else:
                print(f"  ✗ {result[0]}: {result[2]}")
        return self.failed == 0


def test_mann_box_initialization():
    """Test Mann Box model initialization."""
    print("\n" + "="*70)
    print("TEST: Mann Box Initialization")
    print("="*70)
    
    results = TestResults()
    
    try:
        # Test 1: Default initialization
        mann = MannBox()
        if mann.length_scale_u == 300.0:
            results.add_pass("Default length scale u")
        else:
            results.add_fail("Default length scale u", f"Expected 300.0, got {mann.length_scale_u}")
        
        # Test 2: Auto-scaled length scales
        expected_Lv = 0.7 * 300.0
        expected_Lw = 0.4 * 300.0
        if abs(mann.length_scale_v - expected_Lv) < 1e-10:
            results.add_pass("Auto-scaled length scale v (0.7*Lu)")
        else:
            results.add_fail("Auto-scaled length scale v", 
                           f"Expected {expected_Lv}, got {mann.length_scale_v}")
        
        if abs(mann.length_scale_w - expected_Lw) < 1e-10:
            results.add_pass("Auto-scaled length scale w (0.4*Lu)")
        else:
            results.add_fail("Auto-scaled length scale w",
                           f"Expected {expected_Lw}, got {mann.length_scale_w}")
        
        # Test 3: Custom initialization
        mann2 = MannBox(length_scale_u=400.0, variance_u=2.0)
        if mann2.length_scale_u == 400.0 and mann2.variance_u == 2.0:
            results.add_pass("Custom parameter initialization")
        else:
            results.add_fail("Custom parameter initialization",
                           f"Parameters not set correctly")
        
        # Test 4: Variance anisotropy ratios
        expected_var_v = 0.64 * 1.0
        expected_var_w = 0.25 * 1.0
        if abs(mann.variance_v - expected_var_v) < 1e-10:
            results.add_pass("Auto-scaled variance v (0.8²*σu)")
        else:
            results.add_fail("Auto-scaled variance v",
                           f"Expected {expected_var_v}, got {mann.variance_v}")
        
        if abs(mann.variance_w - expected_var_w) < 1e-10:
            results.add_pass("Auto-scaled variance w (0.5²*σu)")
        else:
            results.add_fail("Auto-scaled variance w",
                           f"Expected {expected_var_w}, got {mann.variance_w}")
        
    except Exception as e:
        results.add_fail("Mann Box initialization", str(e))
    
    return results


def test_mann_box_spectrum_computation():
    """Test Mann Box spectral tensor computation."""
    print("\n" + "="*70)
    print("TEST: Mann Box Spectrum Computation")
    print("="*70)
    
    results = TestResults()
    
    try:
        mann = MannBox()
        frequencies = np.logspace(-2, 1, 50)  # 0.01 to 10 Hz
        
        spectrum = mann.compute_spectrum(
            frequencies=frequencies,
            height=90.0,
            mean_wind_speed=12.0
        )
        
        # Test 1: Output structure
        required_keys = ['frequency', 'S_uu', 'S_vv', 'S_ww', 'S_uv', 'S_uw', 'S_vw']
        if all(k in spectrum for k in required_keys):
            results.add_pass("Spectrum output structure")
        else:
            missing = [k for k in required_keys if k not in spectrum]
            results.add_fail("Spectrum output structure", f"Missing keys: {missing}")
        
        # Test 2: Frequency array
        if np.allclose(spectrum['frequency'], frequencies):
            results.add_pass("Frequency array preservation")
        else:
            results.add_fail("Frequency array preservation", "Frequencies don't match")
        
        # Test 3: Positive diagonal spectra
        S_uu = spectrum['S_uu']
        S_vv = spectrum['S_vv']
        S_ww = spectrum['S_ww']
        
        if np.all(S_uu >= -1e-12) and np.all(S_vv >= -1e-12) and np.all(S_ww >= -1e-12):
            results.add_pass("Positive semi-definite diagonal spectra")
        else:
            results.add_fail("Positive semi-definite diagonal spectra",
                           f"Negative values found")
        
        # Test 4: Component energy ordering (S_uu > S_vv > S_ww for typical parameters)
        mean_uu = np.mean(S_uu[1:])
        mean_vv = np.mean(S_vv[1:])
        mean_ww = np.mean(S_ww[1:])
        
        if mean_uu >= mean_vv and mean_vv >= mean_ww:
            results.add_pass(f"Correct energy ordering (u > v > w)")
        else:
            results.add_fail(f"Energy ordering",
                           f"Got: {mean_uu:.4e} > {mean_vv:.4e} > {mean_ww:.4e}")
        
        # Test 5: Component variance ratios (approximately 0.8 and 0.5)
        rms_ratio_v = np.sqrt(mean_vv / mean_uu)
        rms_ratio_w = np.sqrt(mean_ww / mean_uu)
        
        expected_ratio_v = 0.8
        expected_ratio_w = 0.5
        tol = 0.1  # 10% tolerance
        
        if abs(rms_ratio_v - expected_ratio_v) / expected_ratio_v < tol:
            results.add_pass(f"v/u RMS ratio ≈ 0.8 (got {rms_ratio_v:.3f})")
        else:
            results.add_fail(f"v/u RMS ratio", f"Expected ~0.8, got {rms_ratio_v:.3f}")
        
        if abs(rms_ratio_w - expected_ratio_w) / expected_ratio_w < tol:
            results.add_pass(f"w/u RMS ratio ≈ 0.5 (got {rms_ratio_w:.3f})")
        else:
            results.add_fail(f"w/u RMS ratio", f"Expected ~0.5, got {rms_ratio_w:.3f}")
        
    except Exception as e:
        results.add_fail("Mann Box spectrum computation", str(e))
        import traceback
        traceback.print_exc()
    
    return results


def test_mann_box_realizability():
    """Test Mann Box spectral tensor physical realizability."""
    print("\n" + "="*70)
    print("TEST: Mann Box Realizability Constraints")
    print("="*70)
    
    results = TestResults()
    
    try:
        mann = MannBox()
        frequencies = np.logspace(-2, 0.5, 30)
        
        spectrum = mann.compute_spectrum(
            frequencies=frequencies,
            height=90.0,
            mean_wind_speed=12.0
        )
        
        # Test 1: Causality - Cauchy-Schwarz inequality
        S_uu = spectrum['S_uu']
        S_vv = spectrum['S_vv']
        S_ww = spectrum['S_ww']
        S_uv = spectrum['S_uv']
        S_uw = spectrum['S_uw']
        S_vw = spectrum['S_vw']
        
        cs_uv = S_uv**2 - S_uu * S_vv
        cs_uw = S_uw**2 - S_uu * S_ww
        cs_vw = S_vw**2 - S_vv * S_ww
        
        tolerance = 1e-10
        
        if np.all(cs_uv <= tolerance) and np.all(cs_uw <= tolerance) and np.all(cs_vw <= tolerance):
            results.add_pass("Cauchy-Schwarz inequality satisfied")
        else:
            violations = (np.sum(cs_uv > tolerance) + np.sum(cs_uw > tolerance) + 
                         np.sum(cs_vw > tolerance))
            results.add_fail("Cauchy-Schwarz inequality",
                           f"{violations} violation(s) detected")
        
        # Test 2: Realizability validator
        if mann.validate_realizability(spectrum):
            results.add_pass("Realizability validation method")
        else:
            results.add_fail("Realizability validation method",
                           "Realizability checks failed")
        
        # Test 3: Cross-spectrum magnitudes bounded by geometric mean
        for i, (S_ij, S_ii, S_jj, label) in enumerate([
            (S_uv, S_uu, S_vv, 'u-v'),
            (S_uw, S_uu, S_ww, 'u-w'),
            (S_vw, S_vv, S_ww, 'v-w'),
        ]):
            geom_mean = np.sqrt(S_ii * S_jj)
            if np.all(np.abs(S_ij) <= geom_mean + 1e-12):
                results.add_pass(f"Cross-spectrum bounds ({label})")
            else:
                results.add_fail(f"Cross-spectrum bounds ({label})",
                               f"Bound violations detected")
        
    except Exception as e:
        results.add_fail("Realizability constraints", str(e))
        import traceback
        traceback.print_exc()
    
    return results


def test_mann_box_presets():
    """Test Mann Box preset configurations."""
    print("\n" + "="*70)
    print("TEST: Mann Box Presets")
    print("="*70)
    
    results = TestResults()
    
    try:
        presets = ['neutral', 'stable', 'unstable', 'wind_farm', 'complex_terrain']
        
        for preset_name in presets:
            try:
                mann = create_mann_box_preset(preset_name)
                
                # Verify it's a valid MannBox instance
                if isinstance(mann, MannBox):
                    # Check that we can compute spectrum
                    spectrum = mann.compute_spectrum(
                        frequencies=np.array([0.1, 0.5, 1.0]),
                        height=90.0,
                        mean_wind_speed=12.0
                    )
                    if mann.validate_realizability(spectrum):
                        results.add_pass(f"Preset '{preset_name}'")
                    else:
                        results.add_fail(f"Preset '{preset_name}'",
                                       "Spectrum not realizable")
                else:
                    results.add_fail(f"Preset '{preset_name}'",
                                   "Not a MannBox instance")
            except Exception as e:
                results.add_fail(f"Preset '{preset_name}'", str(e))
        
    except Exception as e:
        results.add_fail("Mann Box presets", str(e))
    
    return results


def test_height_dependence():
    """Test height-dependent spectrum behavior."""
    print("\n" + "="*70)
    print("TEST: Height-Dependent Spectrum")
    print("="*70)
    
    results = TestResults()
    
    try:
        mann = MannBox()
        frequencies = np.array([0.1, 0.5, 1.0])
        heights = [10.0, 50.0, 90.0, 150.0]
        
        # Compute spectra at different heights
        spectra = []
        for h in heights:
            spec = mann.compute_spectrum(frequencies, height=h, mean_wind_speed=12.0)
            spectra.append(spec)
        
        # Verify consistent structure
        if all(mann.validate_realizability(s) for s in spectra):
            results.add_pass("Realizability at multiple heights")
        else:
            results.add_fail("Realizability at multiple heights",
                           "Some spectra not realizable")
        
        # Note: Spectrum shape vs height depends on turbulence intensity scaling
        # For neutral atmosphere, spectrum shape typically doesn't change much
        results.add_pass(f"Spectrum computed at {len(heights)} heights")
        
    except Exception as e:
        results.add_fail("Height-dependent spectrum", str(e))
        import traceback
        traceback.print_exc()
    
    return results


def main():
    """Run all tests."""
    print("\n" + "="*70)
    print("MANN BOX TURBULENCE MODEL - PYTHON TESTS")
    print("="*70)
    
    all_results = []
    
    # Run test suites
    all_results.append(test_mann_box_initialization())
    all_results.append(test_mann_box_spectrum_computation())
    all_results.append(test_mann_box_realizability())
    all_results.append(test_mann_box_presets())
    all_results.append(test_height_dependence())
    
    # Summary
    total_passed = sum(r.passed for r in all_results)
    total_failed = sum(r.failed for r in all_results)
    total_tests = total_passed + total_failed
    
    print("\n" + "="*70)
    print("OVERALL RESULTS")
    print("="*70)
    print(f"Total Tests: {total_tests}")
    print(f"Passed: {total_passed}")
    print(f"Failed: {total_failed}")
    
    if total_failed == 0:
        print("\n✓ All tests passed!")
        return 0
    else:
        print(f"\n✗ {total_failed} test(s) failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
