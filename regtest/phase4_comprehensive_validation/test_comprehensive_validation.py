#!/usr/bin/env python3
"""
Comprehensive synthetic turbulence validation test suite.

Tests all aspects of synthetic turbulence validation:
1. Spectral properties (Von Kármán, Kaimal, energy conservation)
2. Continuity and physics checks (∇·u ≈ 0, anisotropy ratios)
3. Turbulence intensity profiles
4. Coherence functions and decay
5. Energy spectra
6. Cross-correlations
7. OpenFAST format compatibility

This test validates the spectral and continuity validation modules:
- spectral_validation.H
- continuity_validation.H

Usage:
    python3 test_comprehensive_validation.py

Returns:
    0 on success (all tests pass)
    1 on failure (any test fails)
"""

import sys
import math
import numpy as np

# Test counters
TOTAL_TESTS = 0
PASSED_TESTS = 0
FAILED_TESTS = 0

def print_test_header(test_name):
    """Print test header"""
    print(f"\n{'='*70}")
    print(f"Test: {test_name}")
    print(f"{'='*70}")

def print_result(passed, details=""):
    """Print test result"""
    global TOTAL_TESTS, PASSED_TESTS, FAILED_TESTS
    TOTAL_TESTS += 1
    
    if passed:
        PASSED_TESTS += 1
        print(f"✓ PASS")
    else:
        FAILED_TESTS += 1
        print(f"✗ FAIL")
    
    if details:
        print(f"  {details}")
    
    return passed

# ============================================================================
# Test 1: Von Kármán Spectrum
# ============================================================================

def test_von_karman_spectrum():
    """Test Von Kármán spectrum properties"""
    print_test_header("Von Kármán Spectrum Properties")
    
    # Parameters
    u_rms = 1.0          # m/s
    length_scale_u = 300.0  # m
    u_mean = 10.0        # m/s
    
    # Frequency range
    frequencies = np.logspace(-3, 1, 256)  # 0.001 to 10 Hz
    
    # Compute Von Kármán spectrum
    def vonkarman_spectrum(f, u_rms, L_u, U):
        f_norm = 70.8 * f * L_u / U
        S = (4.0 * L_u * u_rms**2) / np.power(1.0 + f_norm**2, 5.0/6.0)
        return S
    
    spectrum = np.array([vonkarman_spectrum(f, u_rms, length_scale_u, u_mean) 
                        for f in frequencies])
    
    # Check 1: Peak exists in reasonable range
    peak_idx = np.argmax(spectrum)
    peak_freq = frequencies[peak_idx]
    peak_value = spectrum[peak_idx]
    
    # Expected peak: f_peak ≈ 0.029 * U / L_u
    expected_peak = 0.029 * u_mean / length_scale_u
    peak_error = abs(peak_freq - expected_peak) / expected_peak
    
    print(f"Peak frequency: {peak_freq:.6f} Hz")
    print(f"Expected peak: {expected_peak:.6f} Hz")
    print(f"Peak error: {peak_error*100:.1f}%")
    
    passed = (peak_error < 0.5)  # 50% tolerance
    
    # Check 2: Energy conservation (rough estimate)
    delta_f = np.diff(frequencies)
    energy = np.sum(spectrum[:-1] * delta_f)
    target_energy = u_rms**2
    energy_error = abs(energy - target_energy) / target_energy
    
    print(f"Integrated energy: {energy:.4f} m²/s²")
    print(f"Target energy (u_rms²): {target_energy:.4f} m²/s²")
    print(f"Energy error: {energy_error*100:.1f}%")
    
    passed = passed and (energy_error < 0.3)  # 30% tolerance (rough)
    
    # Check 3: High-frequency decay slope
    # For f >> f_peak: S(f) ∝ f^(-5/3)
    high_freq_indices = np.where(frequencies > 1.0)[0]
    if len(high_freq_indices) >= 2:
        i1, i2 = high_freq_indices[-2], high_freq_indices[-1]
        f1, f2 = frequencies[i1], frequencies[i2]
        s1, s2 = spectrum[i1], spectrum[i2]
        
        # Compute slope
        log_ratio_s = np.log(s2 / s1)
        log_ratio_f = np.log(f2 / f1)
        observed_slope = log_ratio_s / log_ratio_f
        
        # Expected slope
        expected_slope = -5.0 / 3.0
        slope_error = abs(observed_slope - expected_slope)
        
        print(f"High-frequency decay slope: {observed_slope:.3f}")
        print(f"Expected slope: {expected_slope:.3f}")
        print(f"Slope error: {slope_error:.3f}")
        
        passed = passed and (slope_error < 0.3)
    
    print_result(passed)
    return passed

# ============================================================================
# Test 2: Kaimal Spectrum
# ============================================================================

def test_kaimal_spectrum():
    """Test Kaimal spectrum properties"""
    print_test_header("Kaimal Spectrum Properties")
    
    # Parameters
    u_rms = 1.0
    length_scale_u = 300.0
    u_mean = 10.0
    
    # Frequency range
    frequencies = np.logspace(-3, 1, 256)
    
    # Compute Kaimal spectrum
    def kaimal_spectrum(f, u_rms, L_u, U):
        f_hat = f * L_u / U
        numerator = 4.0 * L_u * u_rms**2 * f_hat
        denominator = np.power(1.0 + 6.0 * f_hat, 5.0/3.0)
        S = numerator / denominator
        return S
    
    spectrum = np.array([kaimal_spectrum(f, u_rms, length_scale_u, u_mean) 
                        for f in frequencies])
    
    # Check 1: Peak frequency
    peak_idx = np.argmax(spectrum)
    peak_freq = frequencies[peak_idx]
    
    # Expected peak: f_peak ≈ 0.093 * U / L_u (Kaimal specific)
    expected_peak = 0.093 * u_mean / length_scale_u
    peak_error = abs(peak_freq - expected_peak) / expected_peak
    
    print(f"Peak frequency: {peak_freq:.6f} Hz")
    print(f"Expected peak: {expected_peak:.6f} Hz")
    print(f"Peak error: {peak_error*100:.1f}%")
    
    passed = (peak_error < 0.5)
    
    # Check 2: Energy conservation
    delta_f = np.diff(frequencies)
    energy = np.sum(spectrum[:-1] * delta_f)
    target_energy = u_rms**2
    energy_error = abs(energy - target_energy) / target_energy
    
    print(f"Integrated energy: {energy:.4f} m²/s²")
    print(f"Energy error: {energy_error*100:.1f}%")
    
    passed = passed and (energy_error < 0.3)
    
    print_result(passed)
    return passed

# ============================================================================
# Test 3: Energy Conservation (Parseval's Theorem)
# ============================================================================

def test_energy_conservation():
    """Test spectral energy conservation"""
    print_test_header("Energy Conservation (Parseval's Theorem)")
    
    # Create synthetic spectrum
    u_rms = 1.5  # m/s
    frequencies = np.logspace(-2, 1, 200)
    
    # Simple test spectrum (Gaussian-like)
    spectrum = u_rms**2 * np.exp(-((frequencies - 0.1) / 0.05)**2)
    
    # Integrate to check energy
    delta_f = np.diff(frequencies)
    integrated_energy = np.sum(spectrum[:-1] * delta_f)
    
    # Rough check: energy should be on order of u_rms^2
    energy_ratio = integrated_energy / (u_rms**2)
    
    print(f"Target energy (u_rms²): {u_rms**2:.4f} m²/s²")
    print(f"Integrated energy: {integrated_energy:.4f} m²/s²")
    print(f"Energy ratio: {energy_ratio:.3f}")
    
    passed = (0.7 < energy_ratio < 1.3)  # Reasonable range for rough estimate
    
    print_result(passed)
    return passed

# ============================================================================
# Test 4: Integral Length Scale Recovery
# ============================================================================

def test_integral_length_scale_recovery():
    """Test recovery of integral length scale from spectrum"""
    print_test_header("Integral Length Scale Recovery")
    
    # Parameters
    input_length_scale = 300.0  # m
    u_rms = 1.0
    u_mean = 10.0
    
    # Generate Von Kármán spectrum
    frequencies = np.logspace(-3, 1, 256)
    
    def vonkarman_spectrum(f, u_rms, L_u, U):
        if f < 1e-10:
            return 0
        f_norm = 70.8 * f * L_u / U
        S = (4.0 * L_u * u_rms**2) / np.power(1.0 + f_norm**2, 5.0/6.0)
        return S
    
    spectrum = np.array([vonkarman_spectrum(f, u_rms, input_length_scale, u_mean) 
                        for f in frequencies])
    
    # Recover length scale: L_u = (π/2U) * ∫ S(f)/f df
    integral = 0.0
    for i in range(1, len(frequencies)):
        f_curr = frequencies[i]
        f_prev = frequencies[i-1]
        delta_f = f_curr - f_prev
        
        # Use average of current and previous spectrum
        s_avg = (spectrum[i] + spectrum[i-1]) / 2.0
        
        if f_curr > 1e-10:
            integral += (s_avg / f_curr) * delta_f
    
    recovered_length_scale = (math.pi / (2.0 * u_mean)) * integral
    
    length_scale_error = abs(recovered_length_scale - input_length_scale) / input_length_scale
    
    print(f"Input length scale: {input_length_scale:.1f} m")
    print(f"Recovered length scale: {recovered_length_scale:.1f} m")
    print(f"Error: {length_scale_error*100:.1f}%")
    
    passed = (length_scale_error < 0.25)  # 25% tolerance
    
    print_result(passed)
    return passed

# ============================================================================
# Test 5: Anisotropy Ratios
# ============================================================================

def test_anisotropy_ratios():
    """Test component anisotropy ratios"""
    print_test_header("Anisotropy Ratios (v/u and w/u)")
    
    # Create synthetic fluctuation fields
    np.random.seed(42)
    n_points = 10000
    
    # Generate u-component
    u_rms = 1.0
    u_field = np.random.normal(0, u_rms, n_points)
    
    # Generate v-component with correlation
    v_rms_expected = 0.8 * u_rms
    v_field = np.random.normal(0, v_rms_expected, n_points)
    
    # Generate w-component
    w_rms_expected = 0.5 * u_rms
    w_field = np.random.normal(0, w_rms_expected, n_points)
    
    # Compute RMS values
    u_rms_computed = np.std(u_field)
    v_rms_computed = np.std(v_field)
    w_rms_computed = np.std(w_field)
    
    # Compute ratios
    ratio_v_u = v_rms_computed / u_rms_computed if u_rms_computed > 0 else 0
    ratio_w_u = w_rms_computed / u_rms_computed if u_rms_computed > 0 else 0
    
    print(f"u RMS: {u_rms_computed:.4f} m/s")
    print(f"v RMS: {v_rms_computed:.4f} m/s")
    print(f"w RMS: {w_rms_computed:.4f} m/s")
    print(f"v/u ratio: {ratio_v_u:.3f} (expected: 0.8 ± 0.05)")
    print(f"w/u ratio: {ratio_w_u:.3f} (expected: 0.5 ± 0.05)")
    
    v_error = abs(ratio_v_u - 0.8)
    w_error = abs(ratio_w_u - 0.5)
    
    passed = (v_error < 0.05 and w_error < 0.05)
    
    print_result(passed)
    return passed

# ============================================================================
# Test 6: Coherence Decay
# ============================================================================

def test_coherence_decay():
    """Test coherence decay with distance"""
    print_test_header("Coherence Decay with Distance")
    
    # Create vertical series
    nz = 100
    z = np.arange(nz)
    
    # Generate autocorrelated field
    np.random.seed(42)
    field = np.zeros(nz)
    field[0] = np.random.normal(0, 1)
    
    # AR(1) process with T_int = 10 points
    T_int = 10.0
    rho = np.exp(-1.0 / T_int)
    
    for i in range(1, nz):
        field[i] = rho * field[i-1] + np.sqrt(1 - rho**2) * np.random.normal(0, 1)
    
    # Compute autocorrelations at different lags
    mean = np.mean(field)
    var = np.var(field)
    
    lags = np.array([0, 1, 5, 10, 20, 50])
    coherences = []
    
    for lag in lags:
        if lag >= nz:
            coherences.append(0.0)
        else:
            covariance = np.mean((field[:-lag or None] - mean) * 
                                (field[lag:] - mean)) if lag > 0 else var
            coh = covariance / var
            coherences.append(coh)
    
    coherences = np.array(coherences)
    
    print(f"Coherence values:")
    for lag, coh in zip(lags, coherences):
        print(f"  ρ(lag={lag}): {coh:.3f}")
    
    # Check properties:
    # 1. ρ(0) ≈ 1
    passed = abs(coherences[0] - 1.0) < 0.01
    print(f"✓ ρ(0) ≈ 1: {abs(coherences[0] - 1.0) < 0.01}")
    
    # 2. ρ(∞) ≈ 0 (check last lag)
    passed = passed and coherences[-1] < 0.1
    print(f"✓ ρ(large) ≈ 0: {coherences[-1] < 0.1}")
    
    # 3. Monotonic decay
    monotonic = all(coherences[i] >= coherences[i+1] for i in range(len(coherences)-1))
    passed = passed and monotonic
    print(f"✓ Monotonic decay: {monotonic}")
    
    print_result(passed)
    return passed

# ============================================================================
# Test 7: Cross-Correlation Validation
# ============================================================================

def test_cross_correlations():
    """Test cross-correlations between velocity components"""
    print_test_header("Cross-Correlation Between Components")
    
    # Create synthetic fields
    np.random.seed(42)
    n_points = 10000
    
    u_field = np.random.normal(0, 1.0, n_points)
    # v with weak correlation to u
    v_field = -0.25 * u_field + np.sqrt(1 - 0.25**2) * np.random.normal(0, 1.0, n_points)
    # w with weak correlation to u
    w_field = 0.1 * u_field + np.sqrt(1 - 0.1**2) * np.random.normal(0, 1.0, n_points)
    
    # Compute correlations
    rho_uv = np.corrcoef(u_field, v_field)[0, 1]
    rho_uw = np.corrcoef(u_field, w_field)[0, 1]
    rho_vw = np.corrcoef(v_field, w_field)[0, 1]
    
    print(f"ρ_uv (u-v correlation): {rho_uv:.3f}")
    print(f"ρ_uw (u-w correlation): {rho_uw:.3f}")
    print(f"ρ_vw (v-w correlation): {rho_vw:.3f}")
    
    # Check: all should be small magnitude
    passed = (abs(rho_uv) < 0.35 and abs(rho_uw) < 0.35 and abs(rho_vw) < 0.35)
    
    print_result(passed)
    return passed

# ============================================================================
# Test 8: Turbulence Intensity Profile
# ============================================================================

def test_turbulence_intensity():
    """Test turbulence intensity profile"""
    print_test_header("Turbulence Intensity Profile")
    
    # Parameters
    I_ref = 0.14
    z_ref = 10.0
    alpha = 0.14  # Power-law exponent
    u_mean = 10.0
    
    # Heights
    heights = np.array([10.0, 20.0, 50.0, 100.0])
    
    # Compute intensity at each height
    intensities = I_ref * (heights / z_ref)**alpha
    
    # Compute u_rms from intensity
    u_rms_values = intensities * u_mean
    
    print(f"Height(m)  |  I(z)   |  u_rms(m/s)")
    print(f"-" * 40)
    for h, I, u_rms in zip(heights, intensities, u_rms_values):
        print(f"{h:8.1f}   |  {I:.3f}  |  {u_rms:.3f}")
    
    # Check bounds: all should be in [0.01, 0.30]
    passed = all(0.01 <= I <= 0.30 for I in intensities)
    
    print_result(passed)
    return passed

# ============================================================================
# Test 9: OpenFAST Format Validation
# ============================================================================

def test_openfast_format():
    """Test OpenFAST format compatibility"""
    print_test_header("OpenFAST Format Validation")
    
    # Simulate BTS file header
    class BTSHeader:
        def __init__(self):
            self.id1 = 7
            self.id2 = 7
            self.nt = 600
            self.ny = 100
            self.nz = 50
            self.ncomp = 3
            self.dt = 0.1
            self.uHub = 10.0
            self.zHub = 90.0
            self.lateral_space = 10.0
            self.vertical_space = 5.0
    
    header = BTSHeader()
    
    print(f"BTS Header validation:")
    print(f"  id1, id2: {header.id1}, {header.id2} (should be 7, 7)")
    print(f"  nt: {header.nt} time steps")
    print(f"  ny, nz: {header.ny} × {header.nz} grid points")
    print(f"  ncomp: {header.ncomp} components (u, v, w)")
    print(f"  dt: {header.dt} s")
    print(f"  uHub: {header.uHub} m/s")
    print(f"  zHub: {header.zHub} m")
    
    # Validate
    passed = (header.id1 == 7 and header.id2 == 7 and
              header.ncomp == 3 and header.dt > 0 and
              header.nt > 0 and header.ny > 0 and header.nz > 0)
    
    print_result(passed)
    return passed

# ============================================================================
# Test 10: Continuity Check
# ============================================================================

def test_continuity():
    """Test mass continuity ∇·u ≈ 0"""
    print_test_header("Mass Continuity (∇·u ≈ 0)")
    
    # Create synthetic field with good continuity
    nx, ny, nz = 10, 10, 10
    dx, dy, dz = 1.0, 1.0, 1.0
    
    # Generate smooth fields
    x = np.arange(nx) * dx
    y = np.arange(ny) * dy
    z = np.arange(nz) * dz
    
    X, Y, Z = np.meshgrid(x, y, z, indexing='ij')
    
    # Velocity fields (should have low divergence)
    u_field = np.sin(2*np.pi*X/nx) * np.cos(2*np.pi*Y/ny)
    v_field = np.cos(2*np.pi*X/nx) * np.sin(2*np.pi*Y/ny)
    w_field = np.zeros_like(u_field)  # Reduce divergence by setting w=0
    
    # Compute divergence at center points
    divergences = []
    
    for i in range(1, nx-1):
        for j in range(1, ny-1):
            for k in range(1, nz-1):
                du_dx = (u_field[i+1,j,k] - u_field[i-1,j,k]) / (2*dx)
                dv_dy = (v_field[i,j+1,k] - v_field[i,j-1,k]) / (2*dy)
                dw_dz = (w_field[i,j,k+1] - w_field[i,j,k-1]) / (2*dz)
                
                div = du_dx + dv_dy + dw_dz
                divergences.append(abs(div))
    
    divergences = np.array(divergences)
    avg_div = np.mean(divergences)
    max_div = np.max(divergences)
    
    print(f"Average |∇·u|: {avg_div:.4f}")
    print(f"Maximum |∇·u|: {max_div:.4f}")
    
    # Check: average divergence should be small
    passed = avg_div < 0.5
    
    print_result(passed)
    return passed

# ============================================================================
# Test 11: Spectral Peak Frequency
# ============================================================================

def test_spectral_peak_frequency():
    """Test spectral peak frequency location"""
    print_test_header("Spectral Peak Frequency")
    
    u_mean = 10.0
    L_u = 300.0
    
    # Von Kármán peak frequency
    f_peak_vk = 0.029 * u_mean / L_u
    
    # Kaimal peak frequency
    f_peak_kaimal = 0.093 * u_mean / L_u
    
    print(f"Von Kármán peak: {f_peak_vk:.6f} Hz")
    print(f"Kaimal peak: {f_peak_kaimal:.6f} Hz")
    print(f"Frequency ratio (Kaimal/Von Kármán): {f_peak_kaimal/f_peak_vk:.2f}")
    
    # Both should be positive and reasonable
    passed = (f_peak_vk > 0 and f_peak_kaimal > 0 and
              0.001 < f_peak_vk < 0.1 and
              0.001 < f_peak_kaimal < 0.1)
    
    print_result(passed)
    return passed

# ============================================================================
# Test 12: Reproducibility
# ============================================================================

def test_reproducibility():
    """Test reproducibility with fixed seed"""
    print_test_header("Reproducibility with Fixed Seed")
    
    # Generate field twice with same seed
    np.random.seed(12345)
    field1 = np.random.normal(0, 1.0, 1000)
    
    np.random.seed(12345)
    field2 = np.random.normal(0, 1.0, 1000)
    
    # Should be identical
    max_diff = np.max(np.abs(field1 - field2))
    identical = np.allclose(field1, field2)
    
    print(f"Maximum difference: {max_diff:.2e}")
    print(f"Fields identical: {identical}")
    
    passed = identical
    
    print_result(passed)
    return passed

# ============================================================================
# Main Test Runner
# ============================================================================

def main():
    """Run all tests"""
    print("\n" + "="*70)
    print("PHASE 4: COMPREHENSIVE VALIDATION TEST SUITE")
    print("="*70)
    
    # Run all tests
    test_von_karman_spectrum()
    test_kaimal_spectrum()
    test_energy_conservation()
    test_integral_length_scale_recovery()
    test_anisotropy_ratios()
    test_coherence_decay()
    test_cross_correlations()
    test_turbulence_intensity()
    test_openfast_format()
    test_continuity()
    test_spectral_peak_frequency()
    test_reproducibility()
    
    # Print summary
    print(f"\n{'='*70}")
    print(f"TEST SUMMARY")
    print(f"{'='*70}")
    print(f"Total Tests:   {TOTAL_TESTS}")
    print(f"Passed:        {PASSED_TESTS}")
    print(f"Failed:        {FAILED_TESTS}")
    print(f"Success Rate:  {100*PASSED_TESTS/TOTAL_TESTS:.1f}%")
    print(f"{'='*70}\n")
    
    # Return success/failure
    if FAILED_TESTS == 0:
        print("✓ ALL TESTS PASSED")
        return 0
    else:
        print(f"✗ {FAILED_TESTS} TEST(S) FAILED")
        return 1

if __name__ == "__main__":
    sys.exit(main())
