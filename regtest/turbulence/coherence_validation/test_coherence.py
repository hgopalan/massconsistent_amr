#!/usr/bin/env python3
"""
Directional Coherence Correlations Validation Tests

This test suite validates the directional coherence matrix computation
for u-v-w velocity component cross-correlations.

Tests cover:
1. Diagonal dominance (auto-coherence = 1)
2. Symmetry (coherence_ij = coherence_ji)
3. Boundedness (0 <= coherence <= 1)
4. Stability effects (coherence modification)
5. Coherence model selection (Gaussian, exponential, power-law)
6. Anisotropy ratios (v/u, w/u component coherence)
7. Height dependence (coherence decay with separation)
8. Frequency dependence
9. Cross-component correlations
10. Physical reasonableness

Usage:
    python3 test_coherence.py

Returns:
    0 on success (all tests pass)
    1 on failure (any test fails)
"""

import sys
import os
import numpy as np
from typing import Dict

# Add parent directory to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src/python'))

from iec61400_models import NormalTurbulenceModel

# Global test counters
TOTAL_TESTS = 0
PASSED_TESTS = 0
FAILED_TESTS = 0

def print_test_header(title: str):
    """Print test header"""
    print(f"\n{'='*70}")
    print(f"Test: {title}")
    print(f"{'='*70}")

def print_result(passed: bool, details: str = "") -> bool:
    """Print and track test result"""
    global TOTAL_TESTS, PASSED_TESTS, FAILED_TESTS
    TOTAL_TESTS += 1
    
    if passed:
        PASSED_TESTS += 1
        print("✓ PASS")
    else:
        FAILED_TESTS += 1
        print("✗ FAIL")
    
    if details:
        print(f"  {details}")
    
    return passed

# ============================================================================
# Test 1: Diagonal Dominance (Auto-Coherence = 1)
# ============================================================================

def test_diagonal_dominance():
    """Test that diagonal elements (auto-coherence) equal 1"""
    print_test_header("Diagonal Dominance (Auto-Coherence = 1)")
    
    ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    heights = np.array([10.0, 30.0, 50.0, 100.0, 150.0])
    coh = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'gaussian')
    
    # Check diagonal of all coherence matrices
    diag_uu = np.diag(coh['coherence_uu'])
    diag_vv = np.diag(coh['coherence_vv'])
    diag_ww = np.diag(coh['coherence_ww'])
    
    # All diagonal elements should be 1.0
    passed = np.allclose(diag_uu, 1.0) and np.allclose(diag_vv, 1.0) and np.allclose(diag_ww, 1.0)
    details = f"UU diag: {diag_uu}, VV diag: {diag_vv}, WW diag: {diag_ww}"
    
    return print_result(passed, details)

# ============================================================================
# Test 2: Symmetry of Coherence Matrices
# ============================================================================

def test_coherence_symmetry():
    """Test that coherence matrices are symmetric (Coh_ij = Coh_ji)"""
    print_test_header("Symmetry of Coherence Matrices")
    
    ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    heights = np.array([10.0, 30.0, 50.0, 100.0, 150.0])
    coh = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'exponential')
    
    # Check symmetry for all coherence matrices
    coh_uu = coh['coherence_uu']
    coh_vv = coh['coherence_vv']
    coh_uv = coh['coherence_uv']
    
    # Compute symmetry error
    sym_error_uu = np.max(np.abs(coh_uu - coh_uu.T))
    sym_error_vv = np.max(np.abs(coh_vv - coh_vv.T))
    sym_error_uv = np.max(np.abs(coh_uv - coh_uv.T))
    
    passed = sym_error_uu < 1e-10 and sym_error_vv < 1e-10 and sym_error_uv < 1e-10
    details = f"Max symmetry errors: UU={sym_error_uu:.2e}, VV={sym_error_vv:.2e}, UV={sym_error_uv:.2e}"
    
    return print_result(passed, details)

# ============================================================================
# Test 3: Boundedness (0 <= Coherence <= 1)
# ============================================================================

def test_coherence_boundedness():
    """Test that all coherence values are in [0, 1]"""
    print_test_header("Coherence Boundedness (0 ≤ Coh ≤ 1)")
    
    ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    heights = np.array([10.0, 30.0, 50.0, 100.0, 150.0])
    coh = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'power-law')
    
    # Check boundedness for all matrices
    matrices = ['coherence_uu', 'coherence_vv', 'coherence_ww', 
                'coherence_uv', 'coherence_uw', 'coherence_vw']
    
    all_bounded = True
    for mat_name in matrices:
        mat = coh[mat_name]
        bounded = np.all(mat >= 0.0) and np.all(mat <= 1.0)
        all_bounded = all_bounded and bounded
        if not bounded:
            print(f"  {mat_name}: Min={np.min(mat):.4f}, Max={np.max(mat):.4f}")
    
    min_vals = {m: np.min(coh[m]) for m in matrices}
    max_vals = {m: np.max(coh[m]) for m in matrices}
    details = f"All values in [0,1]: {all_bounded}"
    
    return print_result(all_bounded, details)

# ============================================================================
# Test 4: Monotonic Decay with Height Separation
# ============================================================================

def test_decay_with_height():
    """Test that coherence decays with height separation"""
    print_test_header("Monotonic Decay with Height Separation")
    
    ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    heights = np.array([10.0, 30.0, 50.0, 100.0, 150.0, 200.0])
    coh = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'gaussian')
    
    coh_uu = coh['coherence_uu']
    
    # Check that first row (coherence from 10m to all heights) decreases
    first_row = coh_uu[0, :]
    # Allow some tolerance, but should generally decrease
    diffs = np.diff(first_row)
    num_decreasing = np.sum(diffs < 0)
    fraction_decreasing = num_decreasing / len(diffs)
    
    # Should have at least 80% decreasing
    passed = fraction_decreasing > 0.8
    details = f"Fraction decreasing: {fraction_decreasing*100:.0f}%, Coherence: {first_row}"
    
    return print_result(passed, details)

# ============================================================================
# Test 5: Gaussian vs Exponential vs Power-Law Models
# ============================================================================

def test_coherence_models():
    """Test different coherence models produce different results"""
    print_test_header("Coherence Model Comparison")
    
    ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    heights = np.array([10.0, 50.0, 100.0, 150.0])
    
    coh_gauss = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'gaussian')
    coh_exp = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'exponential')
    coh_pl = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'power-law')
    
    # Get off-diagonal elements for comparison
    uu_gauss = coh_gauss['coherence_uu'][0, 1]
    uu_exp = coh_exp['coherence_uu'][0, 1]
    uu_pl = coh_pl['coherence_uu'][0, 1]
    
    # Models should produce different results
    passed = not (np.isclose(uu_gauss, uu_exp) and np.isclose(uu_exp, uu_pl))
    details = f"Gaussian: {uu_gauss:.4f}, Exponential: {uu_exp:.4f}, Power-law: {uu_pl:.4f}"
    
    return print_result(passed, details)

# ============================================================================
# Test 6: Anisotropy Ratios (V/U and W/U)
# ============================================================================

def test_anisotropy_ratios():
    """Test that V and W component coherence reflects typical anisotropy"""
    print_test_header("Anisotropy Ratios (V/U, W/U)")
    
    ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    heights = np.array([10.0, 50.0, 100.0])
    coh = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'gaussian')
    
    # Check stored anisotropy ratios
    ratios = coh['anisotropy_ratios']
    v_u_ratio = ratios['v/u']
    w_u_ratio = ratios['w/u']
    
    # Typical atmospheric values: V/U ≈ 0.75, W/U ≈ 0.50
    v_ratio_ok = 0.5 < v_u_ratio < 1.0
    w_ratio_ok = 0.3 < w_u_ratio < 0.7
    
    passed = v_ratio_ok and w_ratio_ok
    details = f"V/U: {v_u_ratio:.3f}, W/U: {w_u_ratio:.3f}"
    
    return print_result(passed, details)

# ============================================================================
# Test 7: Stability Effects on Coherence
# ============================================================================

def test_stability_effects():
    """Test that stability modifies coherence scale"""
    print_test_header("Stability Effects on Coherence")
    
    # Compare neutral with stable conditions
    ntm_neutral = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0,
                                        enable_stability_correction=False)
    
    ntm_stable = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0,
                                       enable_stability_correction=True,
                                       monin_obukhov_length=100.0)
    
    heights = np.array([10.0, 50.0, 100.0, 150.0])
    
    coh_neutral = ntm_neutral.compute_coherence_matrix(heights, 0.1, 10.0, 'gaussian')
    coh_stable = ntm_stable.compute_coherence_matrix(heights, 0.1, 10.0, 'gaussian')
    
    # Stable conditions should have lower coherence scale (faster decay)
    # Check stability factors
    stab_factor_neutral = coh_neutral.get('stability_factor', 1.0)
    stab_factor_stable = coh_stable.get('stability_factor', 1.0)
    
    # Stable should have lower factor
    passed = stab_factor_stable <= stab_factor_neutral
    details = f"Neutral factor: {stab_factor_neutral:.3f}, Stable factor: {stab_factor_stable:.3f}"
    
    return print_result(passed, details)

# ============================================================================
# Test 8: Frequency Dependence
# ============================================================================

def test_frequency_dependence():
    """Test that coherence decreases with frequency"""
    print_test_header("Frequency Dependence of Coherence")
    
    ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    heights = np.array([10.0, 50.0, 100.0])
    
    # Compute at different frequencies
    coh_low = ntm.compute_coherence_matrix(heights, 0.01, 10.0, 'exponential')
    coh_mid = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'exponential')
    coh_high = ntm.compute_coherence_matrix(heights, 1.0, 10.0, 'exponential')
    
    # Get off-diagonal coherence at different frequencies
    uu_low = coh_low['coherence_uu'][0, 1]
    uu_mid = coh_mid['coherence_uu'][0, 1]
    uu_high = coh_high['coherence_uu'][0, 1]
    
    # Higher frequency should have lower coherence
    passed = uu_low >= uu_mid and uu_mid >= uu_high
    details = f"Low freq: {uu_low:.4f}, Mid freq: {uu_mid:.4f}, High freq: {uu_high:.4f}"
    
    return print_result(passed, details)

# ============================================================================
# Test 9: Cross-Component Coherence Properties
# ============================================================================

def test_cross_component_coherence():
    """Test cross-component coherence relationships"""
    print_test_header("Cross-Component Coherence Properties")
    
    ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    heights = np.array([10.0, 50.0, 100.0])
    coh = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'gaussian')
    
    # Cross-components should be weaker than auto-components
    coh_uu = coh['coherence_uu'][0, 1]
    coh_uv = coh['coherence_uv'][0, 1]
    coh_uw = coh['coherence_uw'][0, 1]
    
    # Cross-coherence should be less than auto-coherence
    passed = (coh_uv <= coh_uu) and (coh_uw <= coh_uu)
    details = f"UU: {coh_uu:.4f}, UV: {coh_uv:.4f}, UW: {coh_uw:.4f}"
    
    return print_result(passed, details)

# ============================================================================
# Test 10: Matrix Positive Semidefiniteness (Eigenvalues >= 0)
# ============================================================================

def test_positive_semidefinite():
    """Test that coherence matrices are positive semidefinite"""
    print_test_header("Matrix Positive Semidefiniteness")
    
    ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    heights = np.array([10.0, 30.0, 50.0, 100.0])
    coh = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'gaussian')
    
    # Check eigenvalues for U-component coherence matrix
    coh_uu = coh['coherence_uu']
    eigenvalues = np.linalg.eigvalsh(coh_uu)
    
    # All eigenvalues should be >= 0 (positive semidefinite)
    all_non_negative = np.all(eigenvalues >= -1e-10)  # Allow small numerical errors
    
    min_eigenvalue = np.min(eigenvalues)
    details = f"Min eigenvalue: {min_eigenvalue:.6f}, All ≥ 0: {all_non_negative}"
    
    return print_result(all_non_negative, details)

# ============================================================================
# Main Test Runner
# ============================================================================

def main():
    """Run all tests"""
    print("\n" + "="*70)
    print("Directional Coherence Correlations Validation Tests")
    print("="*70)
    
    # Run all tests
    test_diagonal_dominance()
    test_coherence_symmetry()
    test_coherence_boundedness()
    test_decay_with_height()
    test_coherence_models()
    test_anisotropy_ratios()
    test_stability_effects()
    test_frequency_dependence()
    test_cross_component_coherence()
    test_positive_semidefinite()
    
    # Print summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    print(f"Total Tests:  {TOTAL_TESTS}")
    print(f"Passed:       {PASSED_TESTS} ({100*PASSED_TESTS/TOTAL_TESTS:.1f}%)")
    print(f"Failed:       {FAILED_TESTS} ({100*FAILED_TESTS/TOTAL_TESTS:.1f}%)")
    print("="*70)
    
    return 0 if FAILED_TESTS == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
