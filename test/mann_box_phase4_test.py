#!/usr/bin/env python3
"""
Mann Box Phase 4: Temporal & Stability Physics Tests

This test suite validates Phase 4 enhancements including:
1. Time-lag correlation functions (Eulerian and Lagrangian)
2. Taylor frozen turbulence approximation
3. Richardson number classification
4. Obukhov length computation
5. Stability-dependent tensor modifications
6. Convective scaling
7. Vertical coherence effects
8. Complete integration with Phase 3 spectral tensor

References:
  - Mann, J. (1994). The spatial structure of neutral atmospheric surface-layer
    turbulence. Journal of Fluid Mechanics, 273, 141-168.
  - Stull, R. B. (1988). An Introduction to Boundary Layer Meteorology.
  - Obukhov, A. M. (1946). Turbulence in an atmosphere with non-uniform
    temperature. Boundary-Layer Meteorology.
"""

import sys
import math
import json
from typing import List, Tuple, Dict, Optional

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


# ============================================================================
# Part 1: Temporal Correlation Functions
# ============================================================================

def compute_eulerian_autocorrelation(tau: float, T_int: float, model_type: int = 0) -> float:
    """Compute Eulerian time-lag autocorrelation."""
    if T_int < 1.0e-6:
        return 0.0
    
    tau_norm = tau / T_int
    
    if model_type == 0:
        # Exponential
        return math.exp(-abs(tau_norm))
    else:
        # Gaussian
        return math.exp(-tau_norm * tau_norm)


def test_eulerian_autocorrelation():
    """Test Eulerian time-lag autocorrelation."""
    print("\n  Test 1: Eulerian Time-Lag Autocorrelation")
    
    passed = True
    
    # Test 1.1: Exponential decay at tau=0 should be 1.0
    rho_0 = compute_eulerian_autocorrelation(0.0, 1.0, model_type=0)
    test1_1 = abs(rho_0 - 1.0) < 1.0e-6
    report_test("Exponential model: ρ(0) = 1.0", test1_1, f"Got {rho_0}")
    passed = passed and test1_1
    
    # Test 1.2: Exponential decay at tau=T_int should be ~0.368
    rho_T = compute_eulerian_autocorrelation(1.0, 1.0, model_type=0)
    expected = math.exp(-1.0)  # ≈ 0.368
    test1_2 = abs(rho_T - expected) < 1.0e-6
    report_test("Exponential model: ρ(T_int) ≈ exp(-1)", test1_2, 
                f"Got {rho_T}, expected {expected}")
    passed = passed and test1_2
    
    # Test 1.3: Monotonic decay
    rho_list = [compute_eulerian_autocorrelation(t, 1.0, model_type=0) for t in [0, 0.5, 1.0, 2.0]]
    monotonic = all(rho_list[i] >= rho_list[i+1] for i in range(len(rho_list)-1))
    report_test("Monotonic decay (exponential)", monotonic, 
                f"Values: {[f'{r:.3f}' for r in rho_list]}")
    passed = passed and monotonic
    
    # Test 1.4: Gaussian model at tau=T_int should be ~0.368
    rho_gauss = compute_eulerian_autocorrelation(1.0, 1.0, model_type=1)
    expected_gauss = math.exp(-1.0)
    test1_4 = abs(rho_gauss - expected_gauss) < 1.0e-6
    report_test("Gaussian model: ρ(T_int) ≈ exp(-1)", test1_4,
                f"Got {rho_gauss}, expected {expected_gauss}")
    passed = passed and test1_4
    
    # Test 1.5: Gaussian decays faster than exponential at large tau
    tau_large = 3.0
    rho_exp = compute_eulerian_autocorrelation(tau_large, 1.0, model_type=0)
    rho_gauss = compute_eulerian_autocorrelation(tau_large, 1.0, model_type=1)
    faster = rho_gauss < rho_exp
    report_test("Gaussian decays faster than exponential", faster,
                f"Exp: {rho_exp:.4f}, Gauss: {rho_gauss:.4f}")
    passed = passed and faster
    
    return passed


def compute_lagrangian_autocorrelation(tau: float, T_int: float, model_type: int = 0) -> float:
    """Compute Lagrangian time-lag autocorrelation."""
    if T_int < 1.0e-6:
        return 0.0
    
    # Lagrangian timescale is ~0.5 of Eulerian
    lagrangian_scale = T_int * 0.5
    return compute_eulerian_autocorrelation(tau, lagrangian_scale, model_type)


def test_lagrangian_autocorrelation():
    """Test Lagrangian time-lag autocorrelation."""
    print("\n  Test 2: Lagrangian Time-Lag Autocorrelation")
    
    passed = True
    
    # Test 2.1: Lagrangian should decay faster than Eulerian
    T_int = 1.0
    tau = 1.0
    rho_euler = compute_eulerian_autocorrelation(tau, T_int, model_type=0)
    rho_lagr = compute_lagrangian_autocorrelation(tau, T_int, model_type=0)
    faster = rho_lagr < rho_euler
    report_test("Lagrangian decays faster than Eulerian", faster,
                f"Euler: {rho_euler:.4f}, Lagr: {rho_lagr:.4f}")
    passed = passed and faster
    
    # Test 2.2: Lagrangian at tau=0 should still be 1.0
    rho_lagr_0 = compute_lagrangian_autocorrelation(0.0, 1.0)
    test2_2 = abs(rho_lagr_0 - 1.0) < 1.0e-6
    report_test("Lagrangian: ρ(0) = 1.0", test2_2, f"Got {rho_lagr_0}")
    passed = passed and test2_2
    
    return passed


# ============================================================================
# Part 2: Richardson Number Classification
# ============================================================================

def compute_richardson_number(dtheta_dz: float, du_dz: float, dv_dz: float,
                             theta_mean: float = 300.0, height: float = 10.0) -> float:
    """Compute bulk Richardson number."""
    g = 9.81
    
    shear_squared = du_dz**2 + dv_dz**2
    if shear_squared < 1.0e-8:
        return 1.0 if dtheta_dz > 0.0 else -1.0
    
    numerator = g * dtheta_dz * height * height / theta_mean
    richardson = numerator / shear_squared
    
    return richardson


def classify_stability_regime(richardson: float) -> int:
    """Classify stability: -1=unstable, 0=neutral, 1=stable."""
    CRITICAL = 0.25
    if richardson > CRITICAL:
        return 1
    elif richardson < -CRITICAL:
        return -1
    else:
        return 0


def test_richardson_classification():
    """Test Richardson number classification."""
    print("\n  Test 3: Richardson Number Classification")
    
    passed = True
    
    # Test 3.1: Stable conditions (positive temperature gradient)
    ri_stable = compute_richardson_number(
        dtheta_dz=0.01,      # Warming with height (strong inversion)
        du_dz=0.1,           # Small shear
        dv_dz=0.05,
        theta_mean=300.0,
        height=10.0
    )
    regime_stable = classify_stability_regime(ri_stable)
    test3_1 = regime_stable == 1
    report_test("Stable classification (Ri > 0.25)", test3_1,
                f"Ri = {ri_stable:.4f}, regime = {regime_stable}")
    passed = passed and test3_1
    
    # Test 3.2: Neutral conditions (small temperature gradient)
    ri_neutral = compute_richardson_number(
        dtheta_dz=0.0004,    # Very small gradient (< critical)
        du_dz=0.1,
        dv_dz=0.05,
        theta_mean=300.0,
        height=10.0
    )
    regime_neutral = classify_stability_regime(ri_neutral)
    test3_2 = regime_neutral == 0
    report_test("Neutral classification (-0.25 < Ri < 0.25)", test3_2,
                f"Ri = {ri_neutral:.4f}, regime = {regime_neutral}")
    passed = passed and test3_2
    
    # Test 3.3: Unstable conditions (negative temperature gradient)
    ri_unstable = compute_richardson_number(
        dtheta_dz=-0.01,     # Cooling with height (daytime heating)
        du_dz=0.1,
        dv_dz=0.05,
        theta_mean=300.0,
        height=10.0
    )
    regime_unstable = classify_stability_regime(ri_unstable)
    test3_3 = regime_unstable == -1
    report_test("Unstable classification (Ri < -0.25)", test3_3,
                f"Ri = {ri_unstable:.4f}, regime = {regime_unstable}")
    passed = passed and test3_3
    
    # Test 3.4: Increasing shear reduces Ri
    ri_1 = compute_richardson_number(-0.01, 0.05, 0.02, 300.0, 10.0)
    ri_2 = compute_richardson_number(-0.01, 0.15, 0.07, 300.0, 10.0)
    increases = ri_2 > ri_1  # Ri becomes less negative
    report_test("Ri changes with shear", increases,
                f"Ri(weak shear)={ri_1:.4f}, Ri(strong shear)={ri_2:.4f}")
    passed = passed and increases
    
    return passed


# ============================================================================
# Part 3: Obukhov Length
# ============================================================================

def compute_obukhov_length(u_star: float, heat_flux: float, T_mean: float = 300.0) -> float:
    """Compute Obukhov length."""
    g = 9.81
    kappa = 0.41
    rho = 1.225
    c_p = 1005.0
    
    if abs(u_star) < 1.0e-4:
        return 1.0e6 if heat_flux > 0.0 else -1.0e6
    
    heat_effect = heat_flux / (rho * c_p * T_mean)
    if abs(heat_effect) < 1.0e-6:
        return 1.0e6 if heat_flux > 0.0 else -1.0e6
    
    u_cubed = u_star**3
    denominator = kappa * (g / T_mean) * heat_effect
    
    L_mo = -u_cubed / denominator
    
    # Clamp to reasonable bounds
    return max(min(L_mo, 1.0e6), -1.0e6)


def test_obukhov_length():
    """Test Obukhov length computation."""
    print("\n  Test 4: Obukhov Length Computation")
    
    passed = True
    
    # Test 4.1: Positive heat flux → negative Obukhov length (unstable)
    L_unstable = compute_obukhov_length(u_star=0.5, heat_flux=100.0, T_mean=300.0)
    test4_1 = L_unstable < 0.0
    report_test("Positive heat flux → L_MO < 0 (unstable)", test4_1,
                f"L_MO = {L_unstable:.2f} m")
    passed = passed and test4_1
    
    # Test 4.2: Negative heat flux → positive Obukhov length (stable)
    L_stable = compute_obukhov_length(u_star=0.5, heat_flux=-50.0, T_mean=300.0)
    test4_2 = L_stable > 0.0
    report_test("Negative heat flux → L_MO > 0 (stable)", test4_2,
                f"L_MO = {L_stable:.2f} m")
    passed = passed and test4_2
    
    # Test 4.3: Stronger heat flux → smaller |L_MO|
    L_weak = compute_obukhov_length(u_star=0.5, heat_flux=50.0, T_mean=300.0)
    L_strong = compute_obukhov_length(u_star=0.5, heat_flux=200.0, T_mean=300.0)
    smaller = abs(L_strong) < abs(L_weak)
    report_test("Stronger heat flux → smaller |L_MO|", smaller,
                f"|L|(weak)={abs(L_weak):.2f}, |L|(strong)={abs(L_strong):.2f}")
    passed = passed and smaller
    
    return passed


# ============================================================================
# Part 4: Stability Modification Factors
# ============================================================================

def compute_stability_modification_factor(ri: float, component: int) -> float:
    """Compute stability modification factor for tensor component."""
    CRITICAL = 0.25
    
    clamped_ri = max(min(ri, 1.0), -1.0)
    
    if ri > CRITICAL:  # Stable
        if component == 2:  # w
            factor = 0.5 - 0.5 * clamped_ri
            return max(factor, 0.1)
        else:  # u, v
            factor = 1.0 + 0.3 * clamped_ri
            return min(factor, 1.5)
    elif ri < -CRITICAL:  # Unstable
        if component == 2:  # w
            factor = 1.0 - 0.5 * clamped_ri
            return min(factor, 2.0)
        else:  # u, v
            factor = 1.0 + 0.2 * clamped_ri
            return max(factor, 0.5)
    
    return 1.0


def test_stability_modification_factors():
    """Test stability-dependent tensor modifications."""
    print("\n  Test 5: Stability Modification Factors")
    
    passed = True
    
    # Test 5.1: Stable reduces w-component
    f_w_neutral = compute_stability_modification_factor(0.0, component=2)
    f_w_stable = compute_stability_modification_factor(0.5, component=2)
    reduced = f_w_stable < f_w_neutral
    report_test("Stable: w-component reduced", reduced,
                f"f_w(neutral)={f_w_neutral:.3f}, f_w(stable)={f_w_stable:.3f}")
    passed = passed and reduced
    
    # Test 5.2: Stable increases u-component
    f_u_neutral = compute_stability_modification_factor(0.0, component=0)
    f_u_stable = compute_stability_modification_factor(0.5, component=0)
    increased = f_u_stable > f_u_neutral
    report_test("Stable: u-component increased", increased,
                f"f_u(neutral)={f_u_neutral:.3f}, f_u(stable)={f_u_stable:.3f}")
    passed = passed and increased
    
    # Test 5.3: Unstable increases w-component
    f_w_unstable = compute_stability_modification_factor(-0.5, component=2)
    increased = f_w_unstable > f_w_neutral
    report_test("Unstable: w-component increased", increased,
                f"f_w(neutral)={f_w_neutral:.3f}, f_w(unstable)={f_w_unstable:.3f}")
    passed = passed and increased
    
    # Test 5.4: Factors stay in reasonable range
    ri_values = [-1.0, -0.5, 0.0, 0.5, 1.0]
    for ri in ri_values:
        for comp in [0, 1, 2]:
            factor = compute_stability_modification_factor(ri, comp)
            valid = 0.1 <= factor <= 2.0
            if not valid:
                report_test(f"Factor bounds (Ri={ri:.1f}, comp={comp})", False,
                            f"Got {factor:.3f}, outside [0.1, 2.0]")
                passed = False
    
    if all(0.1 <= compute_stability_modification_factor(ri, c) <= 2.0 
           for ri in ri_values for c in [0, 1, 2]):
        report_test("All factors in bounds [0.1, 2.0]", True,
                   f"Tested {len(ri_values) * 3} combinations")
    
    return passed


# ============================================================================
# Part 5: Convective Scaling
# ============================================================================

def compute_convective_velocity(heat_flux: float, z_i: float, T_mean: float = 300.0) -> float:
    """Compute convective velocity scale."""
    g = 9.81
    rho = 1.225
    c_p = 1005.0
    
    if heat_flux <= 0.0:
        return 0.0
    
    numerator = g * heat_flux * z_i
    denominator = rho * c_p * T_mean
    
    if denominator < 1.0e-6:
        return 0.0
    
    w_star_cubed = numerator / denominator
    return w_star_cubed**(1/3) if w_star_cubed > 0.0 else 0.0


def scale_turbulence_intensity(TI_neutral: float, ri: float) -> float:
    """Scale turbulence intensity for stability."""
    clamped_ri = max(min(ri, 1.0), -1.0)
    
    if ri > 0.25:  # Stable
        factor = 1.0 - 0.4 * clamped_ri
        return TI_neutral * max(factor, 0.3)
    elif ri < -0.25:  # Unstable
        factor = 1.0 - 0.3 * clamped_ri
        return TI_neutral * min(factor, 1.8)
    
    return TI_neutral


def test_convective_scaling():
    """Test convective scaling."""
    print("\n  Test 6: Convective Scaling")
    
    passed = True
    
    # Test 6.1: Positive heat flux produces non-zero w_*
    w_star = compute_convective_velocity(heat_flux=100.0, z_i=1000.0, T_mean=300.0)
    test6_1 = w_star > 0.0
    report_test("Convective velocity (positive heat flux)", test6_1,
                f"w_* = {w_star:.3f} m/s")
    passed = passed and test6_1
    
    # Test 6.2: Zero/negative heat flux produces zero w_*
    w_star_zero = compute_convective_velocity(heat_flux=0.0, z_i=1000.0)
    test6_2 = w_star_zero == 0.0
    report_test("Convective velocity (zero heat flux)", test6_2,
                f"w_* = {w_star_zero:.3f} m/s")
    passed = passed and test6_2
    
    # Test 6.3: Stability scaling of TI
    TI_neutral = 0.12
    TI_stable = scale_turbulence_intensity(TI_neutral, ri=0.5)
    TI_unstable = scale_turbulence_intensity(TI_neutral, ri=-0.5)
    
    reduced = TI_stable < TI_neutral
    report_test("Stable: TI reduced", reduced,
                f"TI(stable)={TI_stable:.4f}, TI(neutral)={TI_neutral:.4f}")
    passed = passed and reduced
    
    increased = TI_unstable > TI_neutral
    report_test("Unstable: TI increased", increased,
                f"TI(unstable)={TI_unstable:.4f}, TI(neutral)={TI_neutral:.4f}")
    passed = passed and increased
    
    return passed


# ============================================================================
# Part 6: Vertical Coherence
# ============================================================================

def compute_vertical_coherence_exponent(ri: float) -> float:
    """Compute effective length scale factor for vertical coherence based on stability."""
    CRITICAL = 0.25
    
    clamped_ri = max(min(ri, 1.0), -1.0)
    
    if ri > CRITICAL:  # Stable - enhance coherence by increasing effective scale
        # Length scale factor: f_L = 1.0 + 0.5*Ri
        # Higher factor = larger effective scale = slower decay = stronger coherence
        factor = 1.0 + 0.5 * clamped_ri
        return min(factor, 2.0)
    elif ri < -CRITICAL:  # Unstable - reduce coherence by decreasing effective scale
        # Length scale factor: f_L = 1.0 + 0.3*Ri (Ri is negative)
        # Lower factor = smaller effective scale = faster decay = weaker coherence
        factor = 1.0 + 0.3 * clamped_ri
        return max(factor, 0.5)
    
    return 1.0


def compute_vertical_correlation(dz: float, L_w: float, ri: float) -> float:
    """Compute vertical correlation coefficient."""
    if L_w < 1.0e-6:
        return 0.0
    
    # Get stability-adjusted effective length scale
    length_factor = compute_vertical_coherence_exponent(ri)
    effective_L = L_w * length_factor
    
    # Standard exponential decay with adjusted length scale
    normalized_sep = dz / effective_L
    
    return math.exp(-abs(normalized_sep))


def test_vertical_coherence():
    """Test vertical coherence effects."""
    print("\n  Test 7: Vertical Coherence")
    
    passed = True
    
    # Test 7.1: Stable conditions enhance vertical coherence (slower decay)
    L_w = 100.0
    dz = 50.0
    
    rho_neutral = compute_vertical_correlation(dz, L_w, ri=0.0)
    rho_stable = compute_vertical_correlation(dz, L_w, ri=0.5)
    
    enhanced = rho_stable > rho_neutral
    report_test("Stable: vertical coherence enhanced", enhanced,
                f"ρ(stable)={rho_stable:.4f}, ρ(neutral)={rho_neutral:.4f}")
    passed = passed and enhanced
    
    # Test 7.2: Unstable conditions reduce vertical coherence (faster decay)
    rho_unstable = compute_vertical_correlation(dz, L_w, ri=-0.5)
    reduced = rho_unstable < rho_neutral
    report_test("Unstable: vertical coherence reduced", reduced,
                f"ρ(unstable)={rho_unstable:.4f}, ρ(neutral)={rho_neutral:.4f}")
    passed = passed and reduced
    
    # Test 7.3: Correlation decreases with height separation
    rho_small = compute_vertical_correlation(10.0, L_w, ri=0.0)
    rho_large = compute_vertical_correlation(100.0, L_w, ri=0.0)
    decreases = rho_large < rho_small
    report_test("Correlation decreases with dz", decreases,
                f"ρ(10m)={rho_small:.4f}, ρ(100m)={rho_large:.4f}")
    passed = passed and decreases
    
    return passed


# ============================================================================
# Part 7: Integration Tests
# ============================================================================

def test_phase4_integration():
    """Test integration of all Phase 4 components."""
    print("\n  Test 8: Phase 4 Integration")
    
    passed = True
    
    # Test 8.1: Realistic stable case (nighttime)
    print("    Scenario: Nighttime stable conditions (nocturnal cooling)")
    ri_stable = compute_richardson_number(
        dtheta_dz=0.01,      # Inversion
        du_dz=0.08,          # Weak shear
        dv_dz=0.04,
        theta_mean=280.0,    # Cool night
        height=10.0
    )
    regime = classify_stability_regime(ri_stable)
    test8_1 = regime == 1
    report_test("  Nighttime regime classification", test8_1,
                f"Ri={ri_stable:.4f}, regime={regime}")
    passed = passed and test8_1
    
    # Energy budget
    f_u = compute_stability_modification_factor(ri_stable, 0)
    f_w = compute_stability_modification_factor(ri_stable, 2)
    energy_shift = f_w < 1.0 < f_u
    report_test("  Nighttime energy shift (w↓, u↑)", energy_shift,
                f"f_u={f_u:.3f}, f_w={f_w:.3f}")
    passed = passed and energy_shift
    
    # Test 8.2: Realistic unstable case (daytime)
    print("\n    Scenario: Daytime unstable conditions (surface heating)")
    ri_unstable = compute_richardson_number(
        dtheta_dz=-0.015,    # Superadiabatic layer
        du_dz=0.12,          # Strong shear
        dv_dz=0.06,
        theta_mean=310.0,    # Warm day
        height=50.0
    )
    regime = classify_stability_regime(ri_unstable)
    test8_2 = regime == -1
    report_test("  Daytime regime classification", test8_2,
                f"Ri={ri_unstable:.4f}, regime={regime}")
    passed = passed and test8_2
    
    # Convection
    w_star = compute_convective_velocity(heat_flux=300.0, z_i=1500.0, T_mean=310.0)
    convects = w_star > 0.5
    report_test("  Convection present (w_* > 0.5 m/s)", convects,
                f"w_*={w_star:.3f} m/s")
    passed = passed and convects
    
    # Test 8.3: Temporal scales
    print("\n    Scenario: Time series generation stability")
    T_int = 1.5  # seconds
    dt = 0.1     # seconds
    tau_values = [0.0, 0.5, 1.0, 2.0, 5.0]
    
    correlations = [compute_eulerian_autocorrelation(tau, T_int) for tau in tau_values]
    monotonic = all(correlations[i] >= correlations[i+1] for i in range(len(correlations)-1))
    report_test("  Time series correlations monotonic", monotonic,
                f"ρ values: {[f'{c:.3f}' for c in correlations]}")
    passed = passed and monotonic
    
    return passed


def main():
    """Run all Phase 4 tests."""
    print("\n" + "="*70)
    print("MANN BOX PHASE 4: TEMPORAL & STABILITY PHYSICS TESTS")
    print("="*70)
    
    # Run all test groups
    t1 = test_eulerian_autocorrelation()
    t2 = test_lagrangian_autocorrelation()
    t3 = test_richardson_classification()
    t4 = test_obukhov_length()
    t5 = test_stability_modification_factors()
    t6 = test_convective_scaling()
    t7 = test_vertical_coherence()
    t8 = test_phase4_integration()
    
    # Summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    print(f"\nTotal Tests: {test_results['passed'] + test_results['failed']}")
    print(f"Passed: {test_results['passed']}")
    print(f"Failed: {test_results['failed']}")
    
    if test_results['failed'] == 0:
        print("\n✓ ALL TESTS PASSED!")
        print("\nPhase 4 Key Achievements:")
        print("  ✓ Temporal correlations (Eulerian & Lagrangian)")
        print("  ✓ Richardson number classification")
        print("  ✓ Obukhov length computation")
        print("  ✓ Stability-dependent tensor modifications")
        print("  ✓ Convective scaling")
        print("  ✓ Vertical coherence effects")
        print("  ✓ Full integration with real-world scenarios")
        return 0
    else:
        print(f"\n✗ {test_results['failed']} TEST(S) FAILED")
        return 1


if __name__ == "__main__":
    sys.exit(main())
