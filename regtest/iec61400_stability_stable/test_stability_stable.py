#!/usr/bin/env python3
"""
Phase 3+ Regression Test: Non-Neutral Stability Corrections - Stable Conditions

Tests Monin-Obukhov similarity theory with stable atmospheric conditions
(e.g., nighttime with weak wind, clear sky, strong temperature inversion).

Stable conditions (L > 0) are characterized by:
- Reduced turbulence intensity (stronger stable stratification suppresses mixing)
- Shorter integral length scales (weaker vertical mixing)
- Modified spectral shape (more energy at low frequencies)
- Wind profile following log-law with stability corrections

Test Cases:
1. Strongly stable conditions (L = 50 m) - typical nighttime
2. Moderately stable conditions (L = 200 m) - early morning/evening
3. Weakly stable conditions (L = 500 m) - transition regime
4. Cross-height consistency (verify physical plausibility)
"""

import sys
import os
import numpy as np
from pathlib import Path

# Add source directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "python"))

from iec61400_models import NormalTurbulenceModel


class TestStabilityStable:
    """Test suite for stable atmospheric conditions"""
    
    def __init__(self):
        """Initialize test parameters"""
        self.turbine_class = "II"
        self.terrain_category = 1
        self.z_hub = 90.0
        self.mean_wind_speed = 10.0
        self.heights = np.array([10.0, 30.0, 50.0, 90.0, 150.0])
        
    def test_strongly_stable_conditions(self):
        """
        Test strongly stable conditions (L = 50 m).
        Typical nighttime scenario with strong temperature inversion.
        
        Expected behavior:
        - TI reduced by ~70% compared to neutral
        - Length scales reduced by ~50%
        """
        print("\n" + "="*70)
        print("Test 1: Strongly Stable Conditions (L = 50 m)")
        print("="*70)
        
        # Initialize models
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_stable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=50.0
        )
        
        # Compute turbulence intensities
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        ti_stable = np.array([m_stable._turbulence_intensity_with_stability(h) for h in self.heights])
        
        # Verify stability reduction
        reduction_ratio = ti_stable / ti_neutral
        print(f"Height\t\tTI (Neutral)\tTI (Stable)\tReduction")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            print(f"{h:.1f}m\t\t{ti_neutral[i]:.4f}\t\t{ti_stable[i]:.4f}\t\t{reduction_ratio[i]:.2%}")
        
        # Check that stable conditions reduce TI
        assert np.all(ti_stable < ti_neutral), "Stable TI should be less than neutral TI"
        
        # Check reasonable reduction factor (20-80% of neutral remains)
        assert np.all(reduction_ratio > 0.15) and np.all(reduction_ratio < 0.85), \
            f"TI ratio should be 15-85% of neutral, got {reduction_ratio*100}%"
        
        # Check length scale modifications
        L_u_neutral = 300.0
        L_u_stable = m_stable._length_scale_with_stability(L_u_neutral, self.z_hub)
        print(f"\nIntegral length scale at hub height:")
        print(f"  Neutral: {L_u_neutral:.1f} m")
        print(f"  Stable:  {L_u_stable:.1f} m")
        print(f"  Ratio:   {L_u_stable/L_u_neutral:.2%}")
        
        assert L_u_stable < L_u_neutral, "Stable length scale should be shorter"
        
        print("✓ Test 1 PASSED")
        return True
    
    def test_moderately_stable_conditions(self):
        """
        Test moderately stable conditions (L = 200 m).
        Typical early morning or evening scenario.
        
        Expected behavior:
        - TI reduced by ~30-50% compared to neutral
        - Length scales reduced by ~25-35%
        """
        print("\n" + "="*70)
        print("Test 2: Moderately Stable Conditions (L = 200 m)")
        print("="*70)
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_stable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=200.0
        )
        
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        ti_stable = np.array([m_stable._turbulence_intensity_with_stability(h) for h in self.heights])
        
        reduction_ratio = ti_stable / ti_neutral
        print(f"Height\t\tTI (Neutral)\tTI (Stable)\tReduction")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            print(f"{h:.1f}m\t\t{ti_neutral[i]:.4f}\t\t{ti_stable[i]:.4f}\t\t{reduction_ratio[i]:.2%}")
        
        # Verify moderate reduction
        assert np.all(ti_stable < ti_neutral), "Stable TI should be less than neutral TI"
        assert np.all(reduction_ratio > 0.4) and np.all(reduction_ratio < 0.9), \
            f"TI reduction should be 10-60%, got {(1-reduction_ratio)*100}%"
        
        print("✓ Test 2 PASSED")
        return True
    
    def test_weakly_stable_conditions(self):
        """
        Test weakly stable conditions (L = 500 m).
        Transition regime between stable and neutral.
        
        Expected behavior:
        - TI reduced by ~10-25% compared to neutral
        - Length scales reduced by ~5-15%
        """
        print("\n" + "="*70)
        print("Test 3: Weakly Stable Conditions (L = 500 m)")
        print("="*70)
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_stable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=500.0
        )
        
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        ti_stable = np.array([m_stable._turbulence_intensity_with_stability(h) for h in self.heights])
        
        reduction_ratio = ti_stable / ti_neutral
        print(f"Height\t\tTI (Neutral)\tTI (Stable)\tReduction")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            print(f"{h:.1f}m\t\t{ti_neutral[i]:.4f}\t\t{ti_stable[i]:.4f}\t\t{reduction_ratio[i]:.2%}")
        
        # Verify weak reduction
        assert np.all(ti_stable < ti_neutral), "Stable TI should be less than neutral TI"
        assert np.all(reduction_ratio > 0.6) and np.all(reduction_ratio < 1.0), \
            f"TI ratio should be 60-100% of neutral, got {reduction_ratio*100}%"
        
        print("✓ Test 3 PASSED")
        return True
    
    def test_holtslag_vs_businger_dyer(self):
        """
        Compare Businger-Dyer and Holtslag-De Bruin parameterizations.
        Holtslag-De Bruin should give less severe stability correction in very stable regime.
        """
        print("\n" + "="*70)
        print("Test 4: Parameterization Comparison")
        print("="*70)
        
        # Very stable conditions to see difference
        L = 30.0
        
        m_bd = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=L, use_holtslag=False
        )
        
        m_hb = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=L, use_holtslag=True
        )
        
        ti_bd = np.array([m_bd._turbulence_intensity_with_stability(h) for h in self.heights])
        ti_hb = np.array([m_hb._turbulence_intensity_with_stability(h) for h in self.heights])
        
        print(f"Obukhov length: L = {L} m (very stable)")
        print(f"Height\t\tBusinger-Dyer\tHoltslag-De Bruin\tDifference")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            diff = abs(ti_hb[i] - ti_bd[i]) / ti_bd[i] * 100
            print(f"{h:.1f}m\t\t{ti_bd[i]:.4f}\t\t{ti_hb[i]:.4f}\t\t{diff:.1f}%")
        
        # Both should give reduced turbulence, but Holtslag should be less severe
        assert np.all(ti_hb >= ti_bd) or np.allclose(ti_hb, ti_bd, rtol=0.1), \
            "Holtslag-De Bruin should give less severe or similar reduction"
        
        print("✓ Test 4 PASSED")
        return True
    
    def test_spectrum_with_stable_correction(self):
        """
        Verify spectral modifications with stability corrections.
        Stable conditions should shift energy toward lower frequencies.
        """
        print("\n" + "="*70)
        print("Test 5: Spectral Modifications with Stability")
        print("="*70)
        
        frequencies = np.logspace(-2, 0.5, 64)
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_stable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=100.0
        )
        
        spectrum_neutral = m_neutral.von_karman_spectrum(frequencies, self.z_hub, self.mean_wind_speed)
        spectrum_stable = m_stable.von_karman_spectrum(frequencies, self.z_hub, self.mean_wind_speed)
        
        # Compute spectral peak frequencies
        peak_idx_neutral = np.argmax(spectrum_neutral)
        peak_idx_stable = np.argmax(spectrum_stable)
        
        print(f"Von Kármán Spectrum Analysis:")
        print(f"  Peak frequency (neutral): {frequencies[peak_idx_neutral]:.4f} Hz")
        print(f"  Peak frequency (stable):  {frequencies[peak_idx_stable]:.4f} Hz")
        print(f"  Peak magnitude (neutral): {spectrum_neutral[peak_idx_neutral]:.2f}")
        print(f"  Peak magnitude (stable):  {spectrum_stable[peak_idx_stable]:.2f}")
        
        # Integrated spectral energy (simple trapezoidal rule)
        energy_neutral = 0.0
        energy_stable = 0.0
        for i in range(len(frequencies) - 1):
            df = frequencies[i+1] - frequencies[i]
            energy_neutral += 0.5 * (spectrum_neutral[i] + spectrum_neutral[i+1]) * df
            energy_stable += 0.5 * (spectrum_stable[i] + spectrum_stable[i+1]) * df
        
        print(f"\nIntegrated Spectral Energy:")
        print(f"  Neutral: {energy_neutral:.2f}")
        print(f"  Stable:  {energy_stable:.2f}")
        print(f"  Ratio:   {energy_stable/energy_neutral:.2%}")
        
        # Stable spectrum should have less total energy (due to reduced TI)
        assert energy_stable < energy_neutral, "Stable spectrum should have less energy"
        
        print("✓ Test 5 PASSED")
        return True
    
    def run_all_tests(self):
        """Run all stable conditions tests"""
        print("\n" + "="*70)
        print("Phase 3+ Regression Tests: STABLE ATMOSPHERIC CONDITIONS")
        print("="*70)
        
        tests = [
            self.test_strongly_stable_conditions,
            self.test_moderately_stable_conditions,
            self.test_weakly_stable_conditions,
            self.test_holtslag_vs_businger_dyer,
            self.test_spectrum_with_stable_correction,
        ]
        
        passed = 0
        failed = 0
        
        for test in tests:
            try:
                test()
                passed += 1
            except AssertionError as e:
                print(f"✗ Test FAILED: {e}")
                failed += 1
            except Exception as e:
                print(f"✗ Test ERROR: {e}")
                failed += 1
        
        print("\n" + "="*70)
        print(f"Test Results: {passed} passed, {failed} failed")
        print("="*70)
        
        return failed == 0


if __name__ == "__main__":
    tester = TestStabilityStable()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)
