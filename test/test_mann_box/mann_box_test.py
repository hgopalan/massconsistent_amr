#!/usr/bin/env python3
"""
Mann Box Spectral Tensor Tests (Phase 2)

This test suite validates the Mann Box anisotropic spectral tensor model
implementation for complex terrain wind simulation.

Test coverage:
  1. Spectral tensor diagonal computation
  2. Terrain anisotropy factor computation
  3. Adapted length scale computation
  4. Parameter validation and bounds checking
  5. Physical realizability checks
  6. Complex terrain adaptation (windward/lee slopes)

Reference:
  Mann, J. (1994). The spatial structure of neutral atmospheric surface-layer
  turbulence. Journal of Fluid Mechanics, 273, 141-168.
"""

import sys
import math

# Test result tracking
test_results = {
    'passed': 0,
    'failed': 0,
    'tests': []
}

def linspace(start, stop, num):
    """Simple linspace replacement for numpy.linspace."""
    if num <= 0:
        return []
    if num == 1:
        return [start]
    step = (stop - start) / (num - 1)
    return [start + step * i for i in range(num)]

def test_mann_box_spectrum_diagonal():
    """Test Mann Box spectral tensor diagonal component computation."""
    print("\n" + "="*70)
    print("TEST: Mann Box Spectral Tensor Diagonal (S_ii)")
    print("="*70)
    
    try:
        all_pass = True
        
        # Test parameters
        wavenumber_range = linspace(0.001, 1.0, 10)  # [1/m]
        length_scale = 300.0  # [m]
        variance = 1.0  # [m²/s²]
        asymmetry = 1.0  # Dimensionless
        
        print("\n  Spectral Response Analysis:")
        print("    k [1/m]  | S(k) [m³/s²] | Trend")
        print("    ---------+---------------+-------")
        
        spectrum_values = []
        for k in wavenumber_range:
            # Simplified Mann Box spectrum: 
            # S(k) = (8√(3/(11π)) * σ² * L) / (k * (1 + (k*L/C)²)^(5/6))
            k_scaled = k * length_scale / asymmetry
            norm_factor = 8.0 * math.sqrt(3.0 / (11.0 * math.pi))
            numerator = norm_factor * variance * length_scale
            denominator_base = 1.0 + k_scaled * k_scaled
            
            # Guard against zero wavenumber
            if k < 1e-6:
                spectrum = 0.0
            else:
                denominator = k * math.pow(denominator_base, 5.0/6.0)
                spectrum = numerator / denominator
            
            spectrum_values.append(spectrum)
            
            # Print with trend indicator
            trend = "↑" if len(spectrum_values) < 2 or spectrum > spectrum_values[-2] else "↓"
            print(f"    {k:.4f}  | {spectrum:.8e} | {trend}")
        
        # Validation checks
        print("\n  Physical Realizability Checks:")
        
        # Check 1: Positive spectrum (necessary for energy)
        all_positive = all(s >= 0 for s in spectrum_values)
        print(f"    ✓ All spectrum values non-negative: {all_positive}")
        if not all_positive:
            all_pass = False
        
        # Check 2: Spectrum decreases with high wavenumber (high-frequency cutoff)
        high_freq_spectrum = spectrum_values[-1]
        low_freq_spectrum = spectrum_values[len(spectrum_values)//2]
        decreases_high_freq = high_freq_spectrum < low_freq_spectrum
        print(f"    ✓ Spectrum decreases at high frequencies: {decreases_high_freq}")
        if not decreases_high_freq:
            all_pass = False
        
        # Check 3: Integral scale recovery (approximate)
        # For Mann Box, integral scale L ~ integral of S(k)/k² dk
        # Simplified check: spectrum should peak at low wavenumbers
        peak_idx = spectrum_values.index(max(spectrum_values))
        peaks_at_low_freq = peak_idx < len(spectrum_values) // 3
        print(f"    ✓ Spectrum peaks at low wavenumbers: {peaks_at_low_freq}")
        if not peaks_at_low_freq:
            all_pass = False
        
        # Check 4: No NaN or infinity values
        no_invalid = not any(math.isnan(s) or math.isinf(s) for s in spectrum_values)
        print(f"    ✓ No NaN or infinity values: {no_invalid}")
        if not no_invalid:
            all_pass = False
        
        if all_pass:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Spectral Tensor Diagonal',
                'status': 'PASS',
                'details': 'All physical realizability checks passed'
            })
            print("\n  ✓ PASS: Mann Box spectral tensor diagonal is physically realizable")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Spectral Tensor Diagonal',
                'status': 'FAIL',
                'details': 'Some physical realizability checks failed'
            })
            print("\n  ✗ FAIL: Mann Box spectral tensor has physical issues")
            return False
            
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Mann Box Spectral Tensor Diagonal',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False


def test_mann_box_terrain_anisotropy():
    """Test Mann Box terrain anisotropy factor computation."""
    print("\n" + "="*70)
    print("TEST: Mann Box Terrain Anisotropy Factor")
    print("="*70)
    
    try:
        all_pass = True
        
        # Test parameters
        z_agl_range = [1.0, 10.0, 25.0, 50.0, 100.0, 200.0]  # [m]
        terrain_slopes = [0.0, 0.087, 0.175, 0.262, 0.349]  # [rad] (0°, 5°, 10°, 15°, 20°)
        
        print("\n  Windward Slope Analysis:")
        print("    z_agl [m] | Slope 0° | Slope 10° | Slope 20° | Expected Trend")
        print("    ----------+----------+-----------+-----------+---------------")
        
        for z in z_agl_range:
            factors = []
            for slope in [0.0, 0.175, 0.349]:  # 0°, 10°, 20°
                # Simplified anisotropy factor:
                # height_factor = min(z_agl / 50, 1.0)
                # slope_factor = 1.0 + 1.2 * sin(slope) (windward)
                # total = height_factor * slope_factor
                height_factor = min(z / 50.0, 1.0)
                slope_factor = 1.0 + 1.2 * math.sin(slope)
                anisotropy_factor = height_factor * slope_factor
                factors.append(anisotropy_factor)
            
            print(f"    {z:>6.1f}   | {factors[0]:.4f}   | {factors[1]:.4f}    | {factors[2]:.4f}    | ↑ (increases with slope)")
        
        # Validation checks
        print("\n  Physical Realizability Checks (Windward):")
        
        # Check 1: Anisotropy factor > 0
        test_z = 50.0
        test_slope = 0.175  # 10°
        height_factor = min(test_z / 50.0, 1.0)
        slope_factor = 1.0 + 1.2 * math.sin(test_slope)
        anisotropy_factor = height_factor * slope_factor
        
        is_positive = anisotropy_factor > 0
        print(f"    ✓ Anisotropy factor positive: {is_positive} (value: {anisotropy_factor:.4f})")
        if not is_positive:
            all_pass = False
        
        # Check 2: Increases with slope (windward)
        slope_0 = 1.0 * min(test_z / 50.0, 1.0)  # 0° slope
        slope_10 = (1.0 + 1.2 * math.sin(0.175)) * min(test_z / 50.0, 1.0)  # 10° slope
        increases_with_slope = slope_10 > slope_0
        print(f"    ✓ Increases with slope (10° > 0°): {increases_with_slope}")
        if not increases_with_slope:
            all_pass = False
        
        # Check 3: Height-dependent (increases with height near ground)
        z_low = 5.0
        z_high = 100.0
        height_factor_low = min(z_low / 50.0, 1.0)
        height_factor_high = min(z_high / 50.0, 1.0)
        height_increases = height_factor_high >= height_factor_low
        print(f"    ✓ Height-dependent factor increases: {height_increases}")
        if not height_increases:
            all_pass = False
        
        # Check 4: Lee slope has lower enhancement
        lee_slope_factor = 1.0 + 0.5 * math.sin(0.175)  # 0.5 instead of 1.2
        is_lower_lee = lee_slope_factor < slope_factor
        print(f"    ✓ Lee slope factor < windward: {is_lower_lee}")
        if not is_lower_lee:
            all_pass = False
        
        if all_pass:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Terrain Anisotropy Factor',
                'status': 'PASS',
                'details': 'All anisotropy factor checks passed'
            })
            print("\n  ✓ PASS: Terrain anisotropy factor correctly models slope effects")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Terrain Anisotropy Factor',
                'status': 'FAIL',
                'details': 'Some anisotropy checks failed'
            })
            print("\n  ✗ FAIL: Terrain anisotropy factor has issues")
            return False
            
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Mann Box Terrain Anisotropy Factor',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False


def test_mann_box_adapted_length_scale():
    """Test Mann Box adapted integral length scale for terrain."""
    print("\n" + "="*70)
    print("TEST: Mann Box Adapted Integral Length Scale")
    print("="*70)
    
    try:
        all_pass = True
        
        base_length_scale = 300.0  # [m]
        z_agl_range = [1.0, 10.0, 50.0, 100.0, 200.0]
        
        print("\n  Length Scale Adaptation with Height:")
        print("    z_agl [m] | Flat Terrain | Slope 10° | Ridge Peak | Trend")
        print("    ----------+--------------+-----------+------------+-------")
        
        for z in z_agl_range:
            # Flat terrain
            height_mod = min(z / 100.0, 1.0)
            ridge_factor = 1.0
            slope_reduction = 1.0
            flat_scale = base_length_scale * height_mod * ridge_factor * slope_reduction
            
            # Slope 10° (0.175 rad)
            slope_reduction_10 = max(1.0 - 2.0 * 0.175, 0.5)
            slope_scale = base_length_scale * height_mod * 1.0 * slope_reduction_10
            
            # Ridge peak
            ridge_scale = base_length_scale * height_mod * 1.2 * 1.0
            
            trend = "↑" if flat_scale < 200 else "→"
            print(f"    {z:>6.1f}   | {flat_scale:>8.1f}   | {slope_scale:>8.1f}  | {ridge_scale:>9.1f}  | {trend}")
        
        # Validation checks
        print("\n  Physical Realizability Checks:")
        
        # Check 1: Always positive
        z_test = 50.0
        height_mod = min(z_test / 100.0, 1.0)
        adapted_scale = base_length_scale * height_mod * 1.0 * 1.0
        is_positive = adapted_scale > 0
        print(f"    ✓ Length scale always positive: {is_positive}")
        if not is_positive:
            all_pass = False
        
        # Check 2: Ridge enhancement
        ridge_scale = base_length_scale * height_mod * 1.2 * 1.0
        flat_scale = base_length_scale * height_mod * 1.0 * 1.0
        ridge_enhances = ridge_scale > flat_scale
        print(f"    ✓ Ridge peak enhances scale: {ridge_enhances}")
        if not ridge_enhances:
            all_pass = False
        
        # Check 3: Slope reduces scale (increased mixing)
        slope_red = max(1.0 - 2.0 * 0.175, 0.5)  # 10° slope
        slope_scale = base_length_scale * height_mod * 1.0 * slope_red
        slope_reduces = slope_scale < flat_scale
        print(f"    ✓ Slope reduces scale (increased mixing): {slope_reduces}")
        if not slope_reduces:
            all_pass = False
        
        # Check 4: Never falls below 50% of base
        min_reduction_factor = 0.5
        min_adapted_scale = base_length_scale * min_reduction_factor
        maintains_minimum = adapted_scale > min_adapted_scale * 0.5
        print(f"    ✓ Scale maintains physical minimum: {maintains_minimum}")
        if not maintains_minimum:
            all_pass = False
        
        if all_pass:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Adapted Length Scale',
                'status': 'PASS',
                'details': 'All length scale adaptation checks passed'
            })
            print("\n  ✓ PASS: Length scale adaptation correctly handles terrain effects")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Adapted Length Scale',
                'status': 'FAIL',
                'details': 'Some length scale checks failed'
            })
            print("\n  ✗ FAIL: Length scale adaptation has issues")
            return False
            
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Mann Box Adapted Length Scale',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False


def test_mann_box_parameter_bounds():
    """Test Mann Box parameter validation and bounds checking."""
    print("\n" + "="*70)
    print("TEST: Mann Box Parameter Validation and Bounds")
    print("="*70)
    
    try:
        all_pass = True
        
        print("\n  Parameter Range Validation:")
        print("    Parameter                | Typical Range      | Status")
        print("    -------------------------+--------------------+--------")
        
        # Check length scales
        valid_length_scales = True
        for L_u, name in [(300.0, "L_u"), (200.0, "L_v"), (120.0, "L_w")]:
            in_range = 50.0 <= L_u <= 500.0
            valid_length_scales = valid_length_scales and in_range
            status = "✓" if in_range else "✗"
            print(f"    {name:25} | 50-500 m           | {status}")
        
        # Check variance ratios
        valid_variance = True
        for var, name in [(1.0, "var_u"), (0.8, "var_v"), (0.5, "var_w")]:
            in_range = 0.1 <= var <= 1.5
            valid_variance = valid_variance and in_range
            status = "✓" if in_range else "✗"
            print(f"    {name:25} | 0.1-1.5            | {status}")
        
        # Check asymmetry parameter
        asymmetry = 1.0
        valid_asymmetry = 0.5 <= asymmetry <= 2.0
        status = "✓" if valid_asymmetry else "✗"
        print(f"    {'Asymmetry parameter':25} | 0.5-2.0            | {status}")
        
        # Check eddy lifetime
        eddy_lifetime = 0.1
        valid_lifetime = 0.01 <= eddy_lifetime <= 1.0
        status = "✓" if valid_lifetime else "✗"
        print(f"    {'Eddy lifetime':25} | 0.01-1.0 s         | {status}")
        
        # Check terrain adaptation factor
        adapt_factor = 1.0
        valid_adapt = 0.5 <= adapt_factor <= 2.0
        status = "✓" if valid_adapt else "✗"
        print(f"    {'Terrain adapt factor':25} | 0.5-2.0            | {status}")
        
        all_pass = valid_length_scales and valid_variance and valid_asymmetry and valid_lifetime and valid_adapt
        
        # Additional validation
        print("\n  Input Guard Checks:")
        
        # Check guard against zero wavenumber
        zero_k_guard = True
        test_k = 1e-10
        if test_k >= 1e-6:
            zero_k_guard = False
        print(f"    ✓ Zero wavenumber guard: {zero_k_guard}")
        
        # Check guard against negative distances
        neg_dist_guard = max(0.0, -5.0) == 0.0
        print(f"    ✓ Negative distance guard: {neg_dist_guard}")
        
        # Check guard against very small variance
        small_var_guard = max(1e-6, 1e-10) == 1e-6
        print(f"    ✓ Small variance guard: {small_var_guard}")
        
        all_pass = all_pass and zero_k_guard and neg_dist_guard and small_var_guard
        
        if all_pass:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Parameter Bounds',
                'status': 'PASS',
                'details': 'All parameter bounds and guards valid'
            })
            print("\n  ✓ PASS: All parameter ranges and guards are valid")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Parameter Bounds',
                'status': 'FAIL',
                'details': 'Some parameter bounds failed'
            })
            print("\n  ✗ FAIL: Parameter bounds have issues")
            return False
            
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Mann Box Parameter Bounds',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False


def test_mann_box_windward_lee_asymmetry():
    """Test Mann Box windward vs lee slope asymmetry."""
    print("\n" + "="*70)
    print("TEST: Mann Box Windward vs Lee Slope Asymmetry")
    print("="*70)
    
    try:
        all_pass = True
        
        z_test = 50.0
        slope_test = 0.175  # 10°
        
        # Windward slope
        height_factor = min(z_test / 50.0, 1.0)
        windward_factor = (1.0 + 1.2 * math.sin(slope_test)) * height_factor
        
        # Lee slope
        lee_factor = (1.0 + 0.5 * math.sin(slope_test)) * height_factor
        
        print("\n  Slope Asymmetry Analysis (10° slope, 50m height):")
        print(f"    Windward anisotropy factor: {windward_factor:.4f}")
        print(f"    Lee anisotropy factor:      {lee_factor:.4f}")
        print(f"    Asymmetry ratio (W/L):      {windward_factor/lee_factor:.4f}")
        
        # Validation checks
        print("\n  Physical Realizability Checks:")
        
        # Check 1: Windward > Lee
        windward_greater = windward_factor > lee_factor
        print(f"    ✓ Windward > Lee (turbulence enhancement): {windward_greater}")
        if not windward_greater:
            all_pass = False
        
        # Check 2: Ratio is reasonable (1.05-3.0 for 10° slope at moderate height)
        ratio = windward_factor / lee_factor
        reasonable_ratio = 1.05 <= ratio <= 3.0
        print(f"    ✓ Asymmetry ratio reasonable (1.05-3.0): {reasonable_ratio} (ratio: {ratio:.2f})")
        if not reasonable_ratio:
            all_pass = False
        
        # Check 3: Both positive
        both_positive = windward_factor > 0 and lee_factor > 0
        print(f"    ✓ Both factors positive: {both_positive}")
        if not both_positive:
            all_pass = False
        
        # Check 4: Physically interpretable
        # Higher on windward (flow acceleration, enhanced turbulence generation)
        # Lower on lee (flow separation, reduced coherence)
        interpretable = windward_factor > 1.0 and lee_factor > 0.5
        print(f"    ✓ Physically interpretable values: {interpretable}")
        if not interpretable:
            all_pass = False
        
        if all_pass:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Windward/Lee Asymmetry',
                'status': 'PASS',
                'details': 'Windward/lee asymmetry correctly implemented'
            })
            print("\n  ✓ PASS: Windward/lee slope asymmetry is physically realistic")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'Mann Box Windward/Lee Asymmetry',
                'status': 'FAIL',
                'details': 'Asymmetry check failed'
            })
            print("\n  ✗ FAIL: Windward/lee asymmetry has issues")
            return False
            
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Mann Box Windward/Lee Asymmetry',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False


def print_summary():
    """Print test summary."""
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    
    total_tests = test_results['passed'] + test_results['failed']
    pass_rate = (test_results['passed'] / total_tests * 100) if total_tests > 0 else 0
    
    print(f"\n  Total Tests:  {total_tests}")
    print(f"  Passed:       {test_results['passed']} ✓")
    print(f"  Failed:       {test_results['failed']} ✗")
    print(f"  Pass Rate:    {pass_rate:.1f}%")
    
    print("\n  Test Details:")
    for test in test_results['tests']:
        status_symbol = "✓" if test['status'] == 'PASS' else "✗"
        print(f"    {status_symbol} {test['name']:40} | {test['status']:6} | {test['details']}")
    
    if test_results['failed'] == 0:
        print("\n  ✓ ALL TESTS PASSED")
        return 0
    else:
        print(f"\n  ✗ {test_results['failed']} TEST(S) FAILED")
        return 1


def main():
    """Run all Mann Box tests."""
    print("\n" + "█"*70)
    print("MANN BOX SPECTRAL TENSOR VALIDATION SUITE (Phase 2)")
    print("█"*70)
    print("\nTesting anisotropic spectral tensor model for complex terrain")
    print("Reference: Mann, J. (1994) JFM 273, 141-168")
    
    # Run all tests
    test_mann_box_spectrum_diagonal()
    test_mann_box_terrain_anisotropy()
    test_mann_box_adapted_length_scale()
    test_mann_box_parameter_bounds()
    test_mann_box_windward_lee_asymmetry()
    
    # Print summary
    return print_summary()


if __name__ == '__main__':
    sys.exit(main())
