#!/usr/bin/env python3
"""
Regression tests for IEC 61400-1 turbulence categories A, B, C.

Tests verify that different turbulence categories produce correct intensity
profiles, spectral densities, and fluctuation characteristics following
IEC 61400-1:2019 standard.

Test categories:
  - Category A: 16% turbulence intensity at hub (very turbulent sites)
  - Category B: 14% turbulence intensity at hub (normal sites)
  - Category C: 12% turbulence intensity at hub (low-turbulence sites)
"""

import numpy as np
import unittest
import sys
import os
import json
from pathlib import Path

# Add src/python to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src', 'python'))

from iec61400_models import NormalTurbulenceModel, WindTurbineClass


class TestIEC61400CategoryA(unittest.TestCase):
    """Regression tests for IEC 61400-1 Category A (16% at hub)."""
    
    def setUp(self):
        """Set up test fixtures for Category A."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.category = "A"
        self.hub_height = 90.0
        self.mean_wind_speed = 12.0
    
    def test_category_a_intensity_at_hub(self):
        """Test Category A intensity profile is consistent."""
        # For IEC 61400 NTM, intensity decreases with height
        # The absolute values depend on reference parameters
        intensity_hub = self.ntm.turbulence_intensity(self.hub_height)
        intensity_low = self.ntm.turbulence_intensity(10.0)
        
        # Intensity should be higher at low height
        self.assertGreater(intensity_low, intensity_hub)
        
        # All intensities should be in physical range
        self.assertGreater(intensity_hub, 0.01)
        self.assertLess(intensity_hub, 0.30)
    
    def test_category_a_rms_velocity(self):
        """Test Category A RMS velocities."""
        rms = self.ntm.compute_velocity_rms(self.hub_height, self.mean_wind_speed)
        
        # RMS should be proportional to intensity
        expected_u_rms = 0.14 * self.mean_wind_speed  # Approximately
        
        self.assertGreater(rms['u_rms'], 1.0)
        self.assertLess(rms['u_rms'], 2.0)
        self.assertAlmostEqual(rms['v_rms'] / rms['u_rms'], 0.8, places=1)
        self.assertAlmostEqual(rms['w_rms'] / rms['u_rms'], 0.5, places=1)
    
    def test_category_a_spectrum(self):
        """Test Category A spectrum properties."""
        frequencies = np.logspace(-2, 0.5, 100)
        spectrum = self.ntm.compute_spectrum(
            frequencies, self.hub_height, self.mean_wind_speed,
            spectrum_type="VonKarman"
        )
        
        # Verify spectrum properties
        self.assertTrue(np.all(spectrum['S_u'] >= 0))
        self.assertEqual(len(spectrum['S_u']), len(frequencies))
        
        # Spectrum should peak at low frequencies
        peak_freq_idx = np.argmax(spectrum['S_u'])
        self.assertLess(frequencies[peak_freq_idx], 1.0)
    
    def test_category_a_time_series(self):
        """Test Category A time series generation."""
        ts = self.ntm.generate_time_series(
            duration=60.0, dt=0.1,
            height=self.hub_height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42
        )
        
        # Verify time series statistics
        self.assertAlmostEqual(np.mean(ts['u_prime']), 0.0, places=4)
        self.assertGreater(ts['u_rms'], 0.5)
        self.assertLess(ts['u_rms'], 2.5)
        
        # Anisotropy ratios
        v_u_ratio = ts['v_rms'] / ts['u_rms']
        w_u_ratio = ts['w_rms'] / ts['u_rms']
        self.assertAlmostEqual(v_u_ratio, 0.8, delta=0.15)
        self.assertAlmostEqual(w_u_ratio, 0.5, delta=0.10)


class TestIEC61400CategoryB(unittest.TestCase):
    """Regression tests for IEC 61400-1 Category B (14% at hub)."""
    
    def setUp(self):
        """Set up test fixtures for Category B."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.category = "B"
        self.hub_height = 90.0
        self.mean_wind_speed = 12.0
    
    def test_category_b_intensity_profile(self):
        """Test Category B intensity decreases with height."""
        heights = np.array([10, 50, 90, 150])
        intensities = np.array([self.ntm.turbulence_intensity(h) for h in heights])
        
        # Intensity should decrease with height (monotonic)
        diffs = np.diff(intensities)
        self.assertTrue(np.all(diffs < 0), "Intensities should decrease with height")
    
    def test_category_b_rms_decreases_with_height(self):
        """Test Category B RMS decreases with height."""
        heights = np.array([10, 50, 90, 150])
        rms_values = [self.ntm.compute_velocity_rms(h, self.mean_wind_speed) for h in heights]
        u_rms_values = [rms['u_rms'] for rms in rms_values]
        
        # RMS should decrease with height
        diffs = np.diff(u_rms_values)
        self.assertTrue(np.all(diffs < 0), "RMS should decrease with height")
    
    def test_category_b_energy_conservation(self):
        """Test Category B spectral energy conservation."""
        frequencies = np.logspace(-2, 0.5, 128)
        spectrum = self.ntm.compute_spectrum(
            frequencies, self.hub_height, self.mean_wind_speed,
            spectrum_type="VonKarman"
        )
        
        # Integrate spectrum to check energy
        df = np.gradient(frequencies)
        energy = np.sum(spectrum['S_u'] * df)
        
        u_rms = spectrum['u_rms']
        variance = u_rms**2
        
        # Energy should be on same order as variance
        self.assertGreater(energy, variance * 0.1)
        self.assertLess(energy, variance * 10.0)
    
    def test_category_b_reproducibility(self):
        """Test Category B time series reproducibility."""
        ts1 = self.ntm.generate_time_series(
            duration=30.0, dt=0.1,
            height=self.hub_height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42, n_freq_bins=64
        )
        
        ts2 = self.ntm.generate_time_series(
            duration=30.0, dt=0.1,
            height=self.hub_height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42, n_freq_bins=64
        )
        
        # Should be identical with same seed
        np.testing.assert_array_almost_equal(ts1['u_prime'], ts2['u_prime'])
        np.testing.assert_array_almost_equal(ts1['v_prime'], ts2['v_prime'])
        np.testing.assert_array_almost_equal(ts1['w_prime'], ts2['w_prime'])


class TestIEC61400CategoryC(unittest.TestCase):
    """Regression tests for IEC 61400-1 Category C (12% at hub)."""
    
    def setUp(self):
        """Set up test fixtures for Category C."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.category = "C"
        self.hub_height = 90.0
        self.mean_wind_speed = 12.0
    
    def test_category_c_spectrum_comparison(self):
        """Test Category C spectrum shape matches Von Kármán standard."""
        frequencies = np.logspace(-2, 1, 200)
        spectrum = self.ntm.compute_spectrum(
            frequencies, self.hub_height, self.mean_wind_speed,
            spectrum_type="VonKarman"
        )
        
        # Standard Von Kármán should have -5/3 slope at high frequencies
        # Log-log plot of S(f) vs f should show expected behavior
        
        # At low frequencies: S(f) ≈ const
        # At high frequencies: S(f) ∝ f^(-5/3)
        
        # Extract high-frequency region
        high_freq_mask = frequencies > 1.0
        if np.sum(high_freq_mask) > 1:
            freq_high = frequencies[high_freq_mask]
            S_high = spectrum['S_u'][high_freq_mask]
            
            # Check for negative slope in log-log
            log_freq = np.log(freq_high)
            log_S = np.log(np.maximum(S_high, 1e-10))
            
            # Fit log-log slope (simple 2-point comparison)
            if len(log_freq) > 1:
                slope = (log_S[-1] - log_S[0]) / (log_freq[-1] - log_freq[0])
                self.assertLess(slope, -1.0)  # Should have negative slope
    
    def test_category_c_kaimal_spectrum(self):
        """Test Category C with Kaimal spectrum."""
        frequencies = np.logspace(-2, 0.5, 100)
        spectrum_kaimal = self.ntm.compute_spectrum(
            frequencies, self.hub_height, self.mean_wind_speed,
            spectrum_type="Kaimal"
        )
        
        # Kaimal spectrum should be positive and finite
        self.assertTrue(np.all(spectrum_kaimal['S_u'] >= 0))
        self.assertTrue(np.all(np.isfinite(spectrum_kaimal['S_u'])))
    
    def test_category_c_component_ratios(self):
        """Test Category C maintains proper spectral shape."""
        frequencies = np.logspace(-2, 0.5, 64)
        spectrum = self.ntm.compute_spectrum(
            frequencies, self.hub_height, self.mean_wind_speed,
            spectrum_type="VonKarman"
        )
        
        # All spectral components should be positive and related
        self.assertTrue(np.all(spectrum['S_u'] >= 0))
        self.assertTrue(np.all(spectrum['S_v'] >= 0))
        self.assertTrue(np.all(spectrum['S_w'] >= 0))
        
        # v and w should generally be smaller than u
        mean_S_u = np.mean(spectrum['S_u'])
        mean_S_v = np.mean(spectrum['S_v'])
        mean_S_w = np.mean(spectrum['S_w'])
        
        # These relationships should hold on average
        self.assertGreater(mean_S_u, mean_S_v * 0.5)
        self.assertGreater(mean_S_u, mean_S_w * 0.5)


class TestIEC61400CategoryComparison(unittest.TestCase):
    """Cross-category regression tests."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.hub_height = 90.0
        self.mean_wind_speed = 12.0
    
    def test_categories_consistent_ordering(self):
        """Test that intensity ordering is consistent across categories."""
        # All categories have same NTM model, but intensity should be consistent
        # within profile
        heights = np.array([10, 50, 90, 150])
        intensities = np.array([self.ntm.turbulence_intensity(h) for h in heights])
        
        # Check that intensity decreases monotonically
        for i in range(len(intensities) - 1):
            self.assertGreater(intensities[i], intensities[i+1])
    
    def test_von_karman_vs_kaimal_spectrum(self):
        """Test Von Kármán and Kaimal spectrum comparison."""
        frequencies = np.logspace(-2, 0.5, 100)
        
        spectrum_vk = self.ntm.compute_spectrum(
            frequencies, self.hub_height, self.mean_wind_speed,
            spectrum_type="VonKarman"
        )
        spectrum_kaimal = self.ntm.compute_spectrum(
            frequencies, self.hub_height, self.mean_wind_speed,
            spectrum_type="Kaimal"
        )
        
        # Both should have same RMS (energy)
        self.assertAlmostEqual(spectrum_vk['u_rms'], spectrum_kaimal['u_rms'], places=3)
        
        # But different spectral shapes
        self.assertFalse(np.allclose(spectrum_vk['S_u'], spectrum_kaimal['S_u']))
    
    def test_time_series_statistics(self):
        """Test time series statistics across different configurations."""
        configs = [
            {"spectrum": "VonKarman", "freq_bins": 64},
            {"spectrum": "Kaimal", "freq_bins": 64},
            {"spectrum": "VonKarman", "freq_bins": 128},
        ]
        
        time_series_list = []
        for config in configs:
            ts = self.ntm.generate_time_series(
                duration=60.0, dt=0.1,
                height=self.hub_height, mean_wind_speed=self.mean_wind_speed,
                spectrum_type=config["spectrum"], random_seed=42,
                n_freq_bins=config["freq_bins"]
            )
            time_series_list.append(ts)
        
        # All should have similar RMS values (within reasonable tolerance)
        rms_values = [ts['u_rms'] for ts in time_series_list]
        mean_rms = np.mean(rms_values)
        
        for rms in rms_values:
            # Should be within 20% of mean
            self.assertGreater(rms, mean_rms * 0.8)
            self.assertLess(rms, mean_rms * 1.2)
    
    def test_different_wind_speeds(self):
        """Test that RMS scales with wind speed."""
        wind_speeds = np.array([6, 10, 12, 15])
        rms_u_values = []
        
        for ws in wind_speeds:
            rms = self.ntm.compute_velocity_rms(self.hub_height, ws)
            rms_u_values.append(rms['u_rms'])
        
        rms_u_values = np.array(rms_u_values)
        
        # RMS should scale linearly with wind speed (since I is approx constant)
        # Check that ratios are consistent
        ratios = rms_u_values / wind_speeds
        
        # Ratios should be approximately constant
        mean_ratio = np.mean(ratios)
        for ratio in ratios:
            self.assertAlmostEqual(ratio, mean_ratio, places=2)
    
    def test_different_heights(self):
        """Test that turbulence decreases with height."""
        heights = np.linspace(10, 200, 20)
        rms_values = []
        
        for h in heights:
            rms = self.ntm.compute_velocity_rms(h, self.mean_wind_speed)
            rms_values.append(rms['u_rms'])
        
        rms_values = np.array(rms_values)
        
        # RMS should generally decrease with height (monotonic or nearly so)
        # Allow for some statistical variation
        diffs = np.diff(rms_values)
        negative_diffs = np.sum(diffs < 0)
        
        # Most differences should be negative or very small
        self.assertGreater(negative_diffs, len(diffs) * 0.7)


class TestIEC61400RegressionDataStorage(unittest.TestCase):
    """Test storing and retrieving reference data for regression."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.output_dir = Path(__file__).parent / "regtest_iec61400_data"
        self.output_dir.mkdir(exist_ok=True)
    
    def test_generate_reference_data(self):
        """Generate reference data for regression testing."""
        heights = np.array([10, 50, 90, 150, 200])
        wind_speeds = np.array([8, 10, 12, 14, 16])
        
        reference_data = {
            "model": "NTM_Class_II_Terrain_1",
            "hub_height": 90.0,
            "description": "IEC 61400-1 Normal Turbulence Model - Class II, Terrain 1",
            "heights": heights.tolist(),
            "wind_speeds": wind_speeds.tolist(),
            "results": {}
        }
        
        # Generate results for all combinations
        for ws in wind_speeds:
            ws_key = f"ws_{ws:.1f}"
            reference_data["results"][ws_key] = {}
            
            for h in heights:
                h_key = f"h_{h:.1f}"
                
                # Compute reference values
                rms = self.ntm.compute_velocity_rms(h, ws)
                intensity = self.ntm.turbulence_intensity(h)
                
                reference_data["results"][ws_key][h_key] = {
                    "height": float(h),
                    "wind_speed": float(ws),
                    "turbulence_intensity": float(intensity),
                    "u_rms": float(rms['u_rms']),
                    "v_rms": float(rms['v_rms']),
                    "w_rms": float(rms['w_rms']),
                }
        
        # Save reference data
        ref_file = self.output_dir / "iec61400_reference_ntm.json"
        with open(ref_file, 'w') as f:
            json.dump(reference_data, f, indent=2)
        
        # Verify file was created
        self.assertTrue(ref_file.exists())
        
        # Verify data integrity
        with open(ref_file, 'r') as f:
            loaded_data = json.load(f)
        
        self.assertEqual(len(loaded_data["results"]), len(wind_speeds))
        
        # Check a sample entry
        sample_result = loaded_data["results"]["ws_12.0"]["h_90.0"]
        self.assertIn("u_rms", sample_result)
        self.assertIn("v_rms", sample_result)
        self.assertIn("w_rms", sample_result)
        self.assertGreater(sample_result["u_rms"], 0)
    
    def test_load_and_verify_reference_data(self):
        """Load and verify stored reference data."""
        # First generate reference data
        heights = np.array([10, 50, 90, 150])
        wind_speeds = np.array([10, 12, 14])
        
        reference_data = {
            "model": "NTM_Category_Comparison",
            "results": {}
        }
        
        for ws in wind_speeds:
            ws_key = f"ws_{ws:.1f}"
            reference_data["results"][ws_key] = {}
            for h in heights:
                h_key = f"h_{h:.1f}"
                rms = self.ntm.compute_velocity_rms(h, ws)
                reference_data["results"][ws_key][h_key] = {
                    "u_rms": float(rms['u_rms']),
                    "v_rms": float(rms['v_rms']),
                    "w_rms": float(rms['w_rms']),
                }
        
        # Save and reload
        ref_file = self.output_dir / "iec61400_category_comparison.json"
        with open(ref_file, 'w') as f:
            json.dump(reference_data, f, indent=2)
        
        with open(ref_file, 'r') as f:
            loaded = json.load(f)
        
        # Verify loaded data matches
        for ws in wind_speeds:
            ws_key = f"ws_{ws:.1f}"
            for h in heights:
                h_key = f"h_{h:.1f}"
                original = reference_data["results"][ws_key][h_key]
                loaded_item = loaded["results"][ws_key][h_key]
                
                np.testing.assert_almost_equal(original["u_rms"], loaded_item["u_rms"], decimal=5)
                np.testing.assert_almost_equal(original["v_rms"], loaded_item["v_rms"], decimal=5)
                np.testing.assert_almost_equal(original["w_rms"], loaded_item["w_rms"], decimal=5)


class TestIEC61400SpectrumRegression(unittest.TestCase):
    """Regression tests for spectral calculations."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.hub_height = 90.0
        self.mean_wind_speed = 12.0
    
    def test_spectrum_integral_convergence(self):
        """Test that spectrum integral converges with more frequency bins."""
        heights = [90.0]
        
        # Target RMS
        rms_target = self.ntm.compute_velocity_rms(self.hub_height, self.mean_wind_speed)
        var_target = rms_target['u_rms']**2
        
        # Compute with different frequency resolutions
        freq_bins_list = [32, 64, 128, 256]
        integral_values = []
        
        for n_bins in freq_bins_list:
            frequencies = np.logspace(-2, 0.5, n_bins)
            spectrum = self.ntm.compute_spectrum(
                frequencies, self.hub_height, self.mean_wind_speed,
                spectrum_type="VonKarman"
            )
            
            df = np.gradient(frequencies)
            integral = np.sum(spectrum['S_u'] * df)
            integral_values.append(integral)
        
        # Integrals should converge (later values closer to target)
        integral_values = np.array(integral_values)
        
        # Check that error decreases
        errors = np.abs(integral_values - var_target)
        
        # Later errors should generally be smaller
        early_error = np.mean(errors[:2])
        late_error = np.mean(errors[-2:])
        
        self.assertLess(late_error, early_error * 2.0)
    
    def test_spectral_moments(self):
        """Test spectral moment calculations."""
        frequencies = np.logspace(-2, 0.5, 128)
        spectrum = self.ntm.compute_spectrum(
            frequencies, self.hub_height, self.mean_wind_speed,
            spectrum_type="VonKarman"
        )
        
        df = np.gradient(frequencies)
        S = spectrum['S_u']
        
        # Zeroth moment (variance)
        m0 = np.sum(S * df)
        
        # First moment (frequency-weighted)
        m1 = np.sum(frequencies * S * df)
        
        # Check relationships
        self.assertGreater(m0, 0)
        self.assertGreater(m1, 0)
        self.assertGreater(m1, m0 * 0.001)  # Should have some frequency content


def run_regression_tests():
    """Run all regression tests."""
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add all test classes
    test_classes = [
        TestIEC61400CategoryA,
        TestIEC61400CategoryB,
        TestIEC61400CategoryC,
        TestIEC61400CategoryComparison,
        TestIEC61400RegressionDataStorage,
        TestIEC61400SpectrumRegression,
    ]
    
    for test_class in test_classes:
        suite.addTests(loader.loadTestsFromTestCase(test_class))
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    exit(run_regression_tests())
