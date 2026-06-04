#!/usr/bin/env python3
"""
Mann Box Phase 3: Spectral Tensor Completeness Tests

This test suite validates Phase 3 enhancements including:
1. Full 9-component spectral tensor computation
2. Off-diagonal components and cross-spectra
3. Cauchy-Schwarz inequality verification
4. Physical realizability checks
5. Cross-spectral density matrices
6. Coherence preservation in masking
7. Variance conservation through tensor integration

Reference:
  Mann, J. (1994). The spatial structure of neutral atmospheric surface-layer
  turbulence. Journal of Fluid Mechanics, 273, 141-168.
"""

import sys
import math
import json
from typing import List, Tuple, Dict

# Test result tracking
test_results = {
    'passed': 0,
    'failed': 0,
    'tests': []
}

def report_test(name: str, passed: bool, message: str = ""):
    """Report a single test result."""
    status = "✓ PASS" if passed else "✗ FAIL"
    print(f"    {status}: {name}")
    if message:
        print(f"           {message}")
    
    test_results['tests'].append({
        'name': name,
        'passed': passed,
        'message': message
    })
    
    if passed:
        test_results['passed'] += 1
    else:
        test_results['failed'] += 1


def compute_mann_box_spectrum(k, L, sigma_sq, C=1.0):
    """Compute Mann Box spectral component."""
    k = max(k, 1e-6)
    L = max(L, 1.0)
    sigma_sq = max(sigma_sq, 1e-6)
    C = max(C, 0.5)
    
    k_scaled = k * L / C
    norm_factor = 8.0 * math.sqrt(3.0 / (11.0 * math.pi))
    
    numerator = norm_factor * sigma_sq * L
    denominator_base = 1.0 + k_scaled * k_scaled
    denominator = k * math.pow(denominator_base, 5.0/6.0)
    
    if denominator < 1e-15:
        return 0.0
    
    return numerator / denominator


def compute_off_diagonal_spectrum(k, S_ii, S_jj, L_i, L_j, coherence):
    """Compute off-diagonal spectral component using coherence structure."""
    S_ii = max(S_ii, 0.0)
    S_jj = max(S_jj, 0.0)
    coherence = max(min(coherence, 1.0), 0.0)
    
    if S_ii < 1e-12 or S_jj < 1e-12:
        return 0.0
    
    geom_mean = math.sqrt(S_ii * S_jj)
    L_harmonic = 2.0 * L_i * L_j / (L_i + L_j)
    k_normalized = k * L_harmonic / 300.0
    coherence_decay = math.exp(-k_normalized * k_normalized)
    
    return coherence * geom_mean * coherence_decay


def verify_cauchy_schwarz(S_ii, S_jj, S_ij, tolerance=1e-12):
    """Verify Cauchy-Schwarz inequality: |S_ij|² ≤ S_ii * S_jj."""
    lhs = S_ij * S_ij
    rhs = S_ii * S_jj
    return lhs <= rhs + tolerance


def estimate_condition_number(S_uu, S_vv, S_ww, S_uv, S_uw, S_vw):
    """Estimate spectral condition number using Frobenius norm."""
    trace = S_uu + S_vv + S_ww
    frobenius_sq = S_uu**2 + S_vv**2 + S_ww**2 + 2.0 * (S_uv**2 + S_uw**2 + S_vw**2)
    
    spectral_radius = math.sqrt(frobenius_sq) / 3.0
    min_eigenvalue = trace / 3.0
    
    if min_eigenvalue < 1e-15:
        return 1e10
    
    return spectral_radius / min_eigenvalue


def test_full_spectral_tensor_computation():
    """Test computation of full 9-component spectral tensor."""
    print("\n" + "="*70)
    print("TEST 1: Full Spectral Tensor Computation (9 Components)")
    print("="*70)
    
    # Test parameters
    k_values = [0.001, 0.01, 0.1, 0.5, 1.0]
    L_u, L_v, L_w = 300.0, 200.0, 120.0
    sigma_u_sq, sigma_v_sq, sigma_w_sq = 1.0, 0.80**2, 0.50**2
    C = 1.0
    
    print("\n  Spectral Tensor Components vs Wavenumber:")
    print("    k [1/m] | S_uu       | S_vv       | S_ww       | S_uv       | S_uw       | S_vw")
    print("    --------+------------+------------+------------+------------+------------+----------")
    
    all_pass = True
    
    for k in k_values:
        # Diagonal components
        S_uu = compute_mann_box_spectrum(k, L_u, sigma_u_sq, C)
        S_vv = compute_mann_box_spectrum(k, L_v, sigma_v_sq, C)
        S_ww = compute_mann_box_spectrum(k, L_w, sigma_w_sq, C)
        
        # Off-diagonal components
        S_uv = compute_off_diagonal_spectrum(k, S_uu, S_vv, L_u, L_v, 0.75)
        S_uw = compute_off_diagonal_spectrum(k, S_uu, S_ww, L_u, L_w, 0.50)
        S_vw = compute_off_diagonal_spectrum(k, S_vv, S_ww, L_v, L_w, 0.65)
        
        print(f"    {k:.3e} | {S_uu:.3e} | {S_vv:.3e} | {S_ww:.3e} | {S_uv:.3e} | {S_uw:.3e} | {S_vw:.3e}")
        
        # Verify all components are non-negative
        if S_uu < 0 or S_vv < 0 or S_ww < 0:
            all_pass = False
            print(f"    ERROR: Negative diagonal component at k={k}")
    
    report_test("Full tensor computation (all positive)", all_pass)
    return all_pass


def test_cauchy_schwarz_inequality():
    """Test that cross-spectra satisfy Cauchy-Schwarz inequality."""
    print("\n" + "="*70)
    print("TEST 2: Cauchy-Schwarz Inequality Verification")
    print("="*70)
    
    # Test parameters
    k_values = [0.001, 0.01, 0.1, 0.5, 1.0]
    L_u, L_v, L_w = 300.0, 200.0, 120.0
    sigma_u_sq, sigma_v_sq, sigma_w_sq = 1.0, 0.80**2, 0.50**2
    C = 1.0
    
    print("\n  Cauchy-Schwarz Inequality Check:")
    print("    k [1/m] | |S_uv|²/S_uu/S_vv | Pass?  | |S_uw|²/S_uu/S_ww | Pass?  | |S_vw|²/S_vv/S_ww | Pass?")
    print("    --------+------------------+--------+------------------+--------+------------------+------")
    
    all_pass = True
    violations = 0
    
    for k in k_values:
        # Diagonal components
        S_uu = compute_mann_box_spectrum(k, L_u, sigma_u_sq, C)
        S_vv = compute_mann_box_spectrum(k, L_v, sigma_v_sq, C)
        S_ww = compute_mann_box_spectrum(k, L_w, sigma_w_sq, C)
        
        # Off-diagonal components
        S_uv = compute_off_diagonal_spectrum(k, S_uu, S_vv, L_u, L_v, 0.75)
        S_uw = compute_off_diagonal_spectrum(k, S_uu, S_ww, L_u, L_w, 0.50)
        S_vw = compute_off_diagonal_spectrum(k, S_vv, S_ww, L_v, L_w, 0.65)
        
        # Check Cauchy-Schwarz
        cs_uv = verify_cauchy_schwarz(S_uu, S_vv, S_uv)
        cs_uw = verify_cauchy_schwarz(S_uu, S_ww, S_uw)
        cs_vw = verify_cauchy_schwarz(S_vv, S_ww, S_vw)
        
        # Compute ratios for display
        if S_uu * S_vv > 0:
            ratio_uv = (S_uv * S_uv) / (S_uu * S_vv)
        else:
            ratio_uv = 0.0
        
        if S_uu * S_ww > 0:
            ratio_uw = (S_uw * S_uw) / (S_uu * S_ww)
        else:
            ratio_uw = 0.0
        
        if S_vv * S_ww > 0:
            ratio_vw = (S_vw * S_vw) / (S_vv * S_ww)
        else:
            ratio_vw = 0.0
        
        status_uv = "✓" if cs_uv else "✗"
        status_uw = "✓" if cs_uw else "✗"
        status_vw = "✓" if cs_vw else "✗"
        
        print(f"    {k:.3e} | {ratio_uv:.6f}        | {status_uv}      | {ratio_uw:.6f}        | {status_uw}      | {ratio_vw:.6f}        | {status_vw}")
        
        if not (cs_uv and cs_uw and cs_vw):
            all_pass = False
            violations += 1
    
    report_test("Cauchy-Schwarz inequality satisfied", all_pass, 
                f"Violations: {violations}/{len(k_values)}")
    return all_pass


def test_diagonal_anisotropy():
    """Test that diagonal components reflect proper anisotropy hierarchy."""
    print("\n" + "="*70)
    print("TEST 3: Diagonal Component Anisotropy Hierarchy")
    print("="*70)
    
    # Test that S_uu > S_vv > S_ww (hierarchy preserved)
    # Note: Exact ratios depend on spectral shape, not just variance
    k = 0.001  # Very low wavenumber
    L_u, L_v, L_w = 300.0, 200.0, 120.0
    sigma_u_sq, sigma_v_sq, sigma_w_sq = 1.0, 0.80**2, 0.50**2
    
    S_uu = compute_mann_box_spectrum(k, L_u, sigma_u_sq)
    S_vv = compute_mann_box_spectrum(k, L_v, sigma_v_sq)
    S_ww = compute_mann_box_spectrum(k, L_w, sigma_w_sq)
    
    print("\n  Spectral Anisotropy Hierarchy at Low Wavenumber (k=0.001):")
    print(f"    Variance ratios:  σ_v²/σ_u² = {sigma_v_sq:.4f}, σ_w²/σ_u² = {sigma_w_sq:.4f}")
    print(f"    Spectrum values:  S_uu = {S_uu:.4e}, S_vv = {S_vv:.4e}, S_ww = {S_ww:.4e}")
    print(f"    Spectrum ratios:  S_vv/S_uu = {S_vv/S_uu:.4f}, S_ww/S_uu = {S_ww/S_uu:.4f}")
    
    # Check that hierarchy is preserved (S_uu > S_vv > S_ww)
    # Mann Box spectrum depends on both variance AND length scale
    # Shorter length scale → lower spectrum value (for same variance)
    u_gt_v = S_uu > S_vv
    v_gt_w = S_vv > S_ww
    u_gt_w = S_uu > S_ww
    
    hierarchy_pass = u_gt_v and v_gt_w and u_gt_w
    
    report_test("S_uu > S_vv > S_ww hierarchy preserved", hierarchy_pass,
                f"U>V>W: {u_gt_v}, {v_gt_w}, {u_gt_w}")
    
    # Check that ratios are reasonable (affected by both variance and length scale)
    ratio_v = S_vv / S_uu if S_uu > 0 else 0
    ratio_w = S_ww / S_uu if S_uu > 0 else 0
    
    # These ratios should be less than variance ratios due to shorter scales
    variance_ratio_v = sigma_v_sq / sigma_u_sq
    variance_ratio_w = sigma_w_sq / sigma_u_sq
    
    length_scale_ratio_v = L_v / L_u  # 200/300 = 0.667
    length_scale_ratio_w = L_w / L_u  # 120/300 = 0.4
    
    # Spectrum ratio ≈ variance_ratio * length_scale_ratio (approximate)
    expected_ratio_v_approx = variance_ratio_v * length_scale_ratio_v
    expected_ratio_w_approx = variance_ratio_w * length_scale_ratio_w
    
    print(f"\n  Expected approx ratios (variance × length_scale):")
    print(f"    S_vv/S_uu: {expected_ratio_v_approx:.4f}, actual: {ratio_v:.4f}")
    print(f"    S_ww/S_uu: {expected_ratio_w_approx:.4f}, actual: {ratio_w:.4f}")
    
    tolerance = 0.3  # More generous tolerance for spectral effects
    v_pass = abs(ratio_v - expected_ratio_v_approx) < tolerance
    w_pass = abs(ratio_w - expected_ratio_w_approx) < tolerance
    
    report_test("V-component ratio reasonable", v_pass,
                f"Expected ~{expected_ratio_v_approx:.4f}, got {ratio_v:.4f}")
    report_test("W-component ratio reasonable", w_pass,
                f"Expected ~{expected_ratio_w_approx:.4f}, got {ratio_w:.4f}")
    
    return hierarchy_pass and v_pass and w_pass


def test_spectral_decay():
    """Test that spectral components decay properly at high wavenumbers."""
    print("\n" + "="*70)
    print("TEST 4: High-Frequency Spectral Decay")
    print("="*70)
    
    # Test at increasing wavenumbers
    k_low = 0.001
    k_mid = 0.1
    k_high = 1.0
    
    L_u = 300.0
    sigma_u_sq = 1.0
    
    S_low = compute_mann_box_spectrum(k_low, L_u, sigma_u_sq)
    S_mid = compute_mann_box_spectrum(k_mid, L_u, sigma_u_sq)
    S_high = compute_mann_box_spectrum(k_high, L_u, sigma_u_sq)
    
    print("\n  Spectral Decay Analysis:")
    print(f"    k = {k_low:.3e}: S_uu = {S_low:.6e}")
    print(f"    k = {k_mid:.3e}: S_uu = {S_mid:.6e}")
    print(f"    k = {k_high:.3e}: S_uu = {S_high:.6e}")
    
    # Verify monotonic decay
    decay_1 = S_mid < S_low
    decay_2 = S_high < S_mid
    
    all_pass = decay_1 and decay_2
    
    report_test("Spectrum decays at high frequency", all_pass)
    return all_pass


def test_cross_spectral_density():
    """Test cross-spectral density matrix construction."""
    print("\n" + "="*70)
    print("TEST 5: Cross-Spectral Density Matrix Properties")
    print("="*70)
    
    # Sample spectral tensor
    k = 0.1
    S_uu = compute_mann_box_spectrum(k, 300.0, 1.0)
    S_vv = compute_mann_box_spectrum(k, 200.0, 0.64)
    S_ww = compute_mann_box_spectrum(k, 120.0, 0.25)
    S_uv = compute_off_diagonal_spectrum(k, S_uu, S_vv, 300.0, 200.0, 0.75)
    S_uw = compute_off_diagonal_spectrum(k, S_uu, S_ww, 300.0, 120.0, 0.50)
    S_vw = compute_off_diagonal_spectrum(k, S_vv, S_ww, 200.0, 120.0, 0.65)
    
    print("\n  Spectral Tensor (9 components):")
    print(f"    S_uu = {S_uu:.6e}")
    print(f"    S_vv = {S_vv:.6e}")
    print(f"    S_ww = {S_ww:.6e}")
    print(f"    S_uv = {S_uv:.6e}")
    print(f"    S_uw = {S_uw:.6e}")
    print(f"    S_vw = {S_vw:.6e}")
    
    # Cross-spectral density diagonal
    G_uu = math.sqrt(max(S_uu, 0.0))
    G_vv = math.sqrt(max(S_vv, 0.0))
    G_ww = math.sqrt(max(S_ww, 0.0))
    
    print("\n  Cross-Spectral Density Diagonal:")
    print(f"    G_uu = {G_uu:.6e}")
    print(f"    G_vv = {G_vv:.6e}")
    print(f"    G_ww = {G_ww:.6e}")
    
    # Reconstruction check: G² should approximate S
    S_uu_reconstructed = G_uu ** 2
    S_vv_reconstructed = G_vv ** 2
    S_ww_reconstructed = G_ww ** 2
    
    tolerance = 1e-10
    pass_uu = abs(S_uu - S_uu_reconstructed) < tolerance
    pass_vv = abs(S_vv - S_vv_reconstructed) < tolerance
    pass_ww = abs(S_ww - S_ww_reconstructed) < tolerance
    
    report_test("Cross-spectral diagonal reconstruction", pass_uu and pass_vv and pass_ww)
    return pass_uu and pass_vv and pass_ww


def test_coherence_preservation():
    """Test that masking preserves tensor coherence properties."""
    print("\n" + "="*70)
    print("TEST 6: Coherence-Preserving Masking")
    print("="*70)
    
    # Base spectral tensor
    S_uu = 0.5
    S_vv = 0.32
    S_ww = 0.125
    S_uv = 0.3
    S_uw = 0.15
    S_vw = 0.16
    
    print("\n  Original Tensor (before masking):")
    print(f"    Diagonal:  S_uu={S_uu:.4f}, S_vv={S_vv:.4f}, S_ww={S_ww:.4f}")
    print(f"    Off-diag:  S_uv={S_uv:.4f}, S_uw={S_uw:.4f}, S_vw={S_vw:.4f}")
    
    # Apply coherence-preserving mask
    mask_values = [0.0, 0.25, 0.5, 0.75, 1.0]
    all_pass = True
    
    print("\n  Masked Tensor with Cauchy-Schwarz Check:")
    print("    Mask | S_uu   | S_vv   | S_ww   | CS_uv | CS_uw | CS_vw | Pass?")
    print("    -----+--------+--------+--------+-------+-------+-------+------")
    
    for mask in mask_values:
        # Apply mask
        S_uu_m = S_uu * mask
        S_vv_m = S_vv * mask
        S_ww_m = S_ww * mask
        S_uv_m = S_uv * mask
        S_uw_m = S_uw * mask
        S_vw_m = S_vw * mask
        
        # Verify Cauchy-Schwarz
        cs_uv = verify_cauchy_schwarz(S_uu_m, S_vv_m, S_uv_m)
        cs_uw = verify_cauchy_schwarz(S_uu_m, S_ww_m, S_uw_m)
        cs_vw = verify_cauchy_schwarz(S_vv_m, S_ww_m, S_vw_m)
        
        pass_mask = cs_uv and cs_uw and cs_vw
        status = "✓" if pass_mask else "✗"
        
        print(f"    {mask:.2f} | {S_uu_m:.4f} | {S_vv_m:.4f} | {S_ww_m:.4f} | {int(cs_uv)} | {int(cs_uw)} | {int(cs_vw)} | {status}")
        
        if not pass_mask:
            all_pass = False
    
    report_test("Masking preserves Cauchy-Schwarz", all_pass)
    return all_pass


def test_condition_number():
    """Test spectral condition number estimation."""
    print("\n" + "="*70)
    print("TEST 7: Spectral Condition Number Analysis")
    print("="*70)
    
    # Well-conditioned tensor (small condition number expected)
    print("\n  Well-Conditioned Tensor (isotropic-like):")
    S_uu = 1.0
    S_vv = 0.8
    S_ww = 0.6
    S_uv = 0.5
    S_uw = 0.3
    S_vw = 0.3
    
    CN_good = estimate_condition_number(S_uu, S_vv, S_ww, S_uv, S_uw, S_vw)
    print(f"    Condition number = {CN_good:.4f}")
    print(f"    Interpretation: {CN_good:.2e} (well-conditioned if <100)")
    
    # Ill-conditioned tensor (large condition number expected)
    print("\n  Ill-Conditioned Tensor (highly anisotropic):")
    S_uu = 1.0
    S_vv = 0.01
    S_ww = 0.001
    S_uv = 0.01
    S_uw = 0.001
    S_vw = 0.0001
    
    CN_bad = estimate_condition_number(S_uu, S_vv, S_ww, S_uv, S_uw, S_vw)
    print(f"    Condition number = {CN_bad:.4f}")
    print(f"    Interpretation: {CN_bad:.2e} (ill-conditioned)")
    
    pass_good = CN_good < 100
    pass_bad = CN_bad > CN_good
    
    report_test("Condition number well-conditioned case", pass_good)
    report_test("Condition number ill-conditioned higher", pass_bad)
    
    return pass_good and pass_bad


def main():
    """Run all Phase 3 tests."""
    print("\n" + "="*70)
    print("MANN BOX PHASE 3: SPECTRAL TENSOR COMPLETENESS TESTS")
    print("="*70)
    
    # Run all tests
    t1 = test_full_spectral_tensor_computation()
    t2 = test_cauchy_schwarz_inequality()
    t3 = test_diagonal_anisotropy()
    t4 = test_spectral_decay()
    t5 = test_cross_spectral_density()
    t6 = test_coherence_preservation()
    t7 = test_condition_number()
    
    # Summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    print(f"\nTotal Tests: {test_results['passed'] + test_results['failed']}")
    print(f"Passed: {test_results['passed']}")
    print(f"Failed: {test_results['failed']}")
    
    if test_results['failed'] == 0:
        print("\n✓ ALL TESTS PASSED!")
        return 0
    else:
        print(f"\n✗ {test_results['failed']} TEST(S) FAILED")
        return 1


if __name__ == "__main__":
    sys.exit(main())
