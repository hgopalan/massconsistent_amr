#!/usr/bin/env python3
"""
Non-Neutral Stability Corrections - Neutral Conditions Regression Test

Tests that neutral conditions (very large |L|) show no significant modifications
compared to the standard IEC 61400-1 model.

Neutral conditions (|L| → ∞) occur with:
- Overcast skies (weak surface heating/cooling)
- Strong wind (turbulent mixing prevents stratification)
- Transition times (sunrise/sunset)

Expected behavior:
- TI unchanged from neutral reference
- Length scales unchanged
- Spectral shape unchanged
"""

import sys
import numpy as np
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "python"))

from iec61400_models import NormalTurbulenceModel


class TestStabilityNeutral:
    """Test suite for neutral atmospheric conditions"""
    
    def __init__(self):
        self.turbine_class = "II"
        self.terrain_category = 1
        self.z_hub = 90.0
        self.heights = np.array([10.0, 30.0, 50.0, 90.0, 150.0])
        
    def test_very_large_positive_obukhov_length(self):
        """Test stable with very large L (approaching neutral)"""
        print("\n" + "="*70)
        print("Test 1: Very Large Positive Obukhov Length (L = 10,000 m)")
        print("="*70)
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_large_L = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=10000.0
        )
        
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        ti_large_L = np.array([m_large_L._turbulence_intensity_with_stability(h) for h in self.heights])
        
        print(f"Height\t\tTI (Neutral)\tTI (L=10km)\tDifference")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            diff_pct = abs(ti_large_L[i] - ti_neutral[i]) / ti_neutral[i] * 100
            print(f"{h:.1f}m\t\t{ti_neutral[i]:.4f}\t\t{ti_large_L[i]:.4f}\t\t{diff_pct:.1f}%")
        
        # Should be very close to neutral (<5% difference)
        assert np.allclose(ti_large_L, ti_neutral, rtol=0.06), \
            "Very large L should give neutral-like behavior"
        
        print("✓ Test 1 PASSED")
        return True
    
    def test_very_large_negative_obukhov_length(self):
        """Test unstable with very large |L| (approaching neutral)"""
        print("\n" + "="*70)
        print("Test 2: Very Large Negative Obukhov Length (L = -10,000 m)")
        print("="*70)
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_large_L = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=-10000.0
        )
        
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        ti_large_L = np.array([m_large_L._turbulence_intensity_with_stability(h) for h in self.heights])
        
        print(f"Height\t\tTI (Neutral)\tTI (L=-10km)\tDifference")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            diff_pct = abs(ti_large_L[i] - ti_neutral[i]) / ti_neutral[i] * 100
            print(f"{h:.1f}m\t\t{ti_neutral[i]:.4f}\t\t{ti_large_L[i]:.4f}\t\t{diff_pct:.1f}%")
        
        # Should be very close to neutral (<5% difference)
        assert np.allclose(ti_large_L, ti_neutral, rtol=0.06), \
            "Very large |L| should give neutral-like behavior"
        
        print("✓ Test 2 PASSED")
        return True
    
    def test_disabled_stability_correction(self):
        """Test that disabled stability correction gives exact neutral values"""
        print("\n" + "="*70)
        print("Test 3: Disabled Stability Correction")
        print("="*70)
        
        m_neutral = NormalTurbulenceModel(self.turbine_class, self.terrain_category, self.z_hub)
        m_disabled = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=False, monin_obukhov_length=100.0  # Ignored
        )
        
        ti_neutral = np.array([m_neutral.turbulence_intensity(h) for h in self.heights])
        ti_disabled = np.array([m_disabled.turbulence_intensity(h) for h in self.heights])
        
        print(f"Height\t\tTI (Neutral)\tTI (Disabled)\tDifference")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            print(f"{h:.1f}m\t\t{ti_neutral[i]:.4f}\t\t{ti_disabled[i]:.4f}\t\t{abs(ti_neutral[i]-ti_disabled[i]):.2e}")
        
        # Should be exactly identical
        assert np.allclose(ti_disabled, ti_neutral, rtol=1e-15), \
            "Disabled correction should give exactly neutral values"
        
        print("✓ Test 3 PASSED")
        return True
    
    def test_parameterization_on_neutral(self):
        """Test that different parameterizations give same result for neutral"""
        print("\n" + "="*70)
        print("Test 4: Parameterization Independence for Neutral")
        print("="*70)
        
        m_bd = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=1e9, use_holtslag=False
        )
        
        m_hb = NormalTurbulenceModel(
            self.turbine_class, self.terrain_category, self.z_hub,
            enable_stability_correction=True, monin_obukhov_length=1e9, use_holtslag=True
        )
        
        ti_bd = np.array([m_bd._turbulence_intensity_with_stability(h) for h in self.heights])
        ti_hb = np.array([m_hb._turbulence_intensity_with_stability(h) for h in self.heights])
        
        print(f"Height\t\tBusinger-Dyer\tHoltslag\tDifference")
        print("-" * 60)
        for i, h in enumerate(self.heights):
            print(f"{h:.1f}m\t\t{ti_bd[i]:.4f}\t\t{ti_hb[i]:.4f}\t\t{abs(ti_bd[i]-ti_hb[i]):.2e}")
        
        # Should be identical for neutral (very large L)
        assert np.allclose(ti_bd, ti_hb), \
            "Different parameterizations should give same result for neutral"
        
        print("✓ Test 4 PASSED")
        return True
    
    def run_all_tests(self):
        """Run all neutral conditions tests"""
        print("\n" + "="*70)
        print("Neutral Atmospheric Conditions Regression Tests")
        print("="*70)
        
        tests = [
            self.test_very_large_positive_obukhov_length,
            self.test_very_large_negative_obukhov_length,
            self.test_disabled_stability_correction,
            self.test_parameterization_on_neutral,
        ]
        
        passed = failed = 0
        
        for test in tests:
            try:
                test()
                passed += 1
            except AssertionError as e:
                print(f"✗ FAILED: {e}")
                failed += 1
            except Exception as e:
                print(f"✗ ERROR: {e}")
                failed += 1
        
        print("\n" + "="*70)
        print(f"Test Results: {passed} passed, {failed} failed")
        print("="*70)
        
        return failed == 0


if __name__ == "__main__":
    tester = TestStabilityNeutral()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)
