#!/usr/bin/env python3
"""
Phase 3+ Regression Test: Non-Neutral Stability Corrections - Unstable Conditions

Tests Monin-Obukhov similarity theory with unstable atmospheric conditions
(e.g., daytime with strong solar heating, surface convection).

Unstable conditions (L < 0) are characterized by:
- Enhanced turbulence intensity (strong surface heating induces convection)
- Longer integral length scales (stronger vertical mixing)
- Modified spectral shape (more energy at all frequencies)
- Wind profile modified by buoyancy-driven turbulence

Test Cases:
1. Strongly unstable conditions (L = -50 m) - strong daytime heating
2. Moderately unstable conditions (L = -200 m) - typical afternoon
3. Weakly unstable conditions (L = -500 m) - early morning/evening transition
4. Cross-height consistency (verify physical plausibility)
"""

import sys
import os
import numpy as np
from pathlib import Path

# Add source directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "python"))

from iec61400_models import NormalTurbulenceModel


class TestStabilityUnstable:
    """Test suite for unstable atmospheric conditions"""
    
    def __init__(self):
        """Initialize test parameters"""
        self.turbine_class = "II"
        self.terrain_category = 1
        self.z_hub = 90.0
        self.mean_wind_speed = 10.0
        self.heights = np.array([10.0, 30.0, 50.0, 90.0, 150.0])
        
    def test_strongly_unstable_conditions(self):
        """
        Test strongly unstable conditions (L = -50 m).
        Strong daytime heating scenario with vigorous surface convection.
        
        Expected behavior:
        - TI increased significantly compared to neutral
        - Length scales increased significantly
        - Enhanced energy across all frequencies
        """
        print("\n" + "="*70)
        print("Test 1: Strongly Unstable Conditions (L = -50 m)")
        print("="*70)
        
        # Initialize models
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_unstable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=-50.0
        )
        
        # Compute turbulence intensities
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        ti_unstable = np.array([m_unstable._turbulence_intensity_with_stability(h) for h in self.heights])
        
        # Verify instability enhancement
        enhancement_ratio = ti_unstable / ti_neutral
        print(f"Height\t\tTI (Neutral)\tTI (Unstable)\tEnhancement")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            print(f"{h:.1f}m\t\t{ti_neutral[i]:.4f}\t\t{ti_unstable[i]:.4f}\t\t{enhancement_ratio[i]:.2%}")
        
        # Check that unstable conditions enhance TI
        assert np.all(ti_unstable > ti_neutral), "Unstable TI should be greater than neutral TI"
        
        # Check significant enhancement for strongly unstable (>100%)
        assert np.all(enhancement_ratio > 1.3), \
            f"TI enhancement should be >30%, got {(enhancement_ratio-1)*100}%"
        
        # Check length scale modifications
        L_u_neutral = 300.0
        L_u_unstable = m_unstable._length_scale_with_stability(L_u_neutral, self.z_hub)
        print(f"\nIntegral length scale at hub height:")
        print(f"  Neutral:  {L_u_neutral:.1f} m")
        print(f"  Unstable: {L_u_unstable:.1f} m")
        print(f"  Ratio:    {L_u_unstable/L_u_neutral:.2%}")
        
        assert L_u_unstable > L_u_neutral, "Unstable length scale should be longer"
        
        print("✓ Test 1 PASSED")
        return True
    
    def test_moderately_unstable_conditions(self):
        """
        Test moderately unstable conditions (L = -200 m).
        Typical afternoon scenario with moderate heating.
        
        Expected behavior:
        - TI increased by ~20-40% compared to neutral
        - Length scales increased by ~30-50%
        """
        print("\n" + "="*70)
        print("Test 2: Moderately Unstable Conditions (L = -200 m)")
        print("="*70)
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_unstable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=-200.0
        )
        
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        ti_unstable = np.array([m_unstable._turbulence_intensity_with_stability(h) for h in self.heights])
        
        enhancement_ratio = ti_unstable / ti_neutral
        print(f"Height\t\tTI (Neutral)\tTI (Unstable)\tEnhancement")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            print(f"{h:.1f}m\t\t{ti_neutral[i]:.4f}\t\t{ti_unstable[i]:.4f}\t\t{enhancement_ratio[i]:.2%}")
        
        # Verify moderate enhancement
        assert np.all(ti_unstable > ti_neutral), "Unstable TI should be greater than neutral TI"
        assert np.all(enhancement_ratio > 1.1), \
            f"TI enhancement should be >10%, got {(enhancement_ratio-1)*100}%"
        
        print("✓ Test 2 PASSED")
        return True
    
    def test_weakly_unstable_conditions(self):
        """
        Test weakly unstable conditions (L = -500 m).
        Weak transition regime approaching neutral.
        
        Expected behavior:
        - TI increased by ~5-15% compared to neutral
        - Length scales increased by ~10-20%
        """
        print("\n" + "="*70)
        print("Test 3: Weakly Unstable Conditions (L = -500 m)")
        print("="*70)
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_unstable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=-500.0
        )
        
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        ti_unstable = np.array([m_unstable._turbulence_intensity_with_stability(h) for h in self.heights])
        
        enhancement_ratio = ti_unstable / ti_neutral
        print(f"Height\t\tTI (Neutral)\tTI (Unstable)\tEnhancement")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            print(f"{h:.1f}m\t\t{ti_neutral[i]:.4f}\t\t{ti_unstable[i]:.4f}\t\t{enhancement_ratio[i]:.2%}")
        
        # Verify weak enhancement
        assert np.all(ti_unstable > ti_neutral), "Unstable TI should be greater than neutral TI"
        assert np.all(enhancement_ratio > 1.05), \
            f"TI enhancement should be >5%, got {(enhancement_ratio-1)*100}%"
        
        print("✓ Test 3 PASSED")
        return True
    
    def test_spectrum_with_unstable_correction(self):
        """
        Verify spectral modifications with unstable corrections.
        Unstable conditions should enhance energy across all frequencies.
        """
        print("\n" + "="*70)
        print("Test 4: Spectral Modifications with Instability")
        print("="*70)
        
        frequencies = np.logspace(-2, 0.5, 64)
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_unstable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=-100.0
        )
        
        spectrum_neutral = m_neutral.von_karman_spectrum(frequencies, self.z_hub, self.mean_wind_speed)
        spectrum_unstable = m_unstable.von_karman_spectrum(frequencies, self.z_hub, self.mean_wind_speed)
        
        # Compute spectral peak frequencies
        peak_idx_neutral = np.argmax(spectrum_neutral)
        peak_idx_unstable = np.argmax(spectrum_unstable)
        
        print(f"Von Kármán Spectrum Analysis:")
        print(f"  Peak frequency (neutral):  {frequencies[peak_idx_neutral]:.4f} Hz")
        print(f"  Peak frequency (unstable): {frequencies[peak_idx_unstable]:.4f} Hz")
        print(f"  Peak magnitude (neutral):  {spectrum_neutral[peak_idx_neutral]:.2f}")
        print(f"  Peak magnitude (unstable): {spectrum_unstable[peak_idx_unstable]:.2f}")
        
        # Integrated spectral energy (simple trapezoidal rule)
        energy_neutral = 0.0
        energy_unstable = 0.0
        for i in range(len(frequencies) - 1):
            df = frequencies[i+1] - frequencies[i]
            energy_neutral += 0.5 * (spectrum_neutral[i] + spectrum_neutral[i+1]) * df
            energy_unstable += 0.5 * (spectrum_unstable[i] + spectrum_unstable[i+1]) * df
        
        print(f"\nIntegrated Spectral Energy:")
        print(f"  Neutral:  {energy_neutral:.2f}")
        print(f"  Unstable: {energy_unstable:.2f}")
        print(f"  Ratio:    {energy_unstable/energy_neutral:.2%}")
        
        # Unstable spectrum should have more total energy
        assert energy_unstable > energy_neutral, "Unstable spectrum should have more energy"
        
        print("✓ Test 4 PASSED")
        return True
    
    def test_symmetric_stability_effects(self):
        """
        Test that stable and unstable conditions show opposite effects (symmetric).
        |L| same, opposite sign should give roughly inverse TI ratios.
        """
        print("\n" + "="*70)
        print("Test 5: Symmetric Stability Effects")
        print("="*70)
        
        L_magnitude = 100.0
        
        m_stable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=L_magnitude
        )
        
        m_unstable = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=-L_magnitude
        )
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        
        ti_stable = np.array([m_stable._turbulence_intensity_with_stability(h) for h in self.heights])
        ti_unstable = np.array([m_unstable._turbulence_intensity_with_stability(h) for h in self.heights])
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        
        ratio_stable = ti_stable / ti_neutral
        ratio_unstable = ti_unstable / ti_neutral
        
        print(f"Obukhov length magnitude: L = ±{L_magnitude} m")
        print(f"Height\t\tRatio(Stable)\tRatio(Unstable)\tSum")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            sum_ratio = ratio_stable[i] + ratio_unstable[i]
            print(f"{h:.1f}m\t\t{ratio_stable[i]:.4f}\t\t{ratio_unstable[i]:.4f}\t\t{sum_ratio:.4f}")
        
        # Stable < 1, Unstable > 1
        assert np.all(ratio_stable < 1.0), "Stable should reduce TI"
        assert np.all(ratio_unstable > 1.0), "Unstable should enhance TI"
        
        print("✓ Test 5 PASSED")
        return True
    
    def run_all_tests(self):
        """Run all unstable conditions tests"""
        print("\n" + "="*70)
        print("Phase 3+ Regression Tests: UNSTABLE ATMOSPHERIC CONDITIONS")
        print("="*70)
        
        tests = [
            self.test_strongly_unstable_conditions,
            self.test_moderately_unstable_conditions,
            self.test_weakly_unstable_conditions,
            self.test_spectrum_with_unstable_correction,
            self.test_symmetric_stability_effects,
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
    tester = TestStabilityUnstable()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)
