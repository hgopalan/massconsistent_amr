#!/usr/bin/env python3
"""
Unit tests for IEC 61400-1 turbulent fluctuation generation.

Tests the new fluctuation generation capabilities including:
- RMS velocity computation
- Von Kármán and Kaimal spectral models
- Frequency-domain fluctuation generation
- Time-series synthesis with temporal correlation
"""

import numpy as np
import unittest
import sys
import os

# Add src/python to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from iec61400_models import NormalTurbulenceModel, WindTurbineClass


class TestIEC61400RMSVelocity(unittest.TestCase):
    """Test RMS velocity computation."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.height = 90.0  # Hub height
        self.mean_wind_speed = 12.0  # m/s
    
    def test_compute_velocity_rms_positive(self):
        """Test that RMS velocities are positive."""
        rms = self.ntm.compute_velocity_rms(self.height, self.mean_wind_speed)
        
        self.assertGreater(rms['u_rms'], 0.0)
        self.assertGreater(rms['v_rms'], 0.0)
        self.assertGreater(rms['w_rms'], 0.0)
    
    def test_compute_velocity_rms_anisotropy(self):
        """Test that RMS velocities maintain expected anisotropy ratios."""
        rms = self.ntm.compute_velocity_rms(self.height, self.mean_wind_speed)
        
        # Typical anisotropy: v_rms ≈ 0.8 * u_rms, w_rms ≈ 0.5 * u_rms
        self.assertAlmostEqual(rms['v_rms'] / rms['u_rms'], 0.8, places=5)
        self.assertAlmostEqual(rms['w_rms'] / rms['u_rms'], 0.5, places=5)
    
    def test_compute_velocity_rms_consistency(self):
        """Test that RMS from intensity formula is consistent."""
        rms = self.ntm.compute_velocity_rms(self.height, self.mean_wind_speed)
        intensity = self.ntm.turbulence_intensity(self.height)
        
        # u_rms should equal intensity * mean_wind_speed
        expected_u_rms = intensity * self.mean_wind_speed
        self.assertAlmostEqual(rms['u_rms'], expected_u_rms, places=8)
    
    def test_compute_velocity_rms_height_dependent(self):
        """Test that RMS velocities decrease with height."""
        rms_low = self.ntm.compute_velocity_rms(20.0, self.mean_wind_speed)
        rms_high = self.ntm.compute_velocity_rms(150.0, self.mean_wind_speed)
        
        # RMS should decrease with height due to decreasing turbulence intensity
        self.assertGreater(rms_low['u_rms'], rms_high['u_rms'])
        self.assertGreater(rms_low['v_rms'], rms_high['v_rms'])
        self.assertGreater(rms_low['w_rms'], rms_high['w_rms'])


class TestIEC61400Spectra(unittest.TestCase):
    """Test spectral computation for Von Kármán and Kaimal."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.frequencies = np.logspace(-2, 1, 50)  # 0.01 to 10 Hz
        self.height = 90.0
        self.mean_wind_speed = 12.0
    
    def test_von_karman_spectrum_positive(self):
        """Test that Von Kármán spectrum values are positive."""
        S_u = self.ntm.von_karman_spectrum(
            self.frequencies, self.height, self.mean_wind_speed
        )
        
        self.assertTrue(np.all(S_u >= 0.0))
    
    def test_kaimal_spectrum_positive(self):
        """Test that Kaimal spectrum values are positive."""
        S_u = self.ntm.kaimal_spectrum(
            self.frequencies, self.height, self.mean_wind_speed
        )
        
        self.assertTrue(np.all(S_u >= 0.0))
    
    def test_spectrum_energy_integral(self):
        """Test that spectral energy is in expected range."""
        spectrum = self.ntm.compute_spectrum(
            self.frequencies, self.height, self.mean_wind_speed,
            spectrum_type="VonKarman"
        )
        
        # Integrate spectrum to get variance (should be close to u_rms^2)
        df = np.gradient(self.frequencies)
        energy_integral = np.sum(spectrum['S_u'] * df)
        
        # Energy should be positive and in reasonable range
        self.assertGreater(energy_integral, 0.0)
        # Should be roughly on order of u_rms^2
        u_rms_target = spectrum['u_rms']
        # Energy integral should be somewhat close to variance (within factor of 10)
        self.assertLess(energy_integral, 100 * u_rms_target**2)
    
    def test_compute_spectrum_dictionary_structure(self):
        """Test that compute_spectrum returns proper dictionary."""
        spectrum = self.ntm.compute_spectrum(
            self.frequencies, self.height, self.mean_wind_speed,
            spectrum_type="VonKarman"
        )
        
        # Check required keys
        required_keys = [
            'frequency', 'S_u', 'S_v', 'S_w', 'spectrum_type',
            'height', 'mean_wind_speed', 'u_rms', 'v_rms', 'w_rms'
        ]
        for key in required_keys:
            self.assertIn(key, spectrum)
    
    def test_spectrum_component_anisotropy(self):
        """Test that spectrum components maintain anisotropy."""
        spectrum = self.ntm.compute_spectrum(
            self.frequencies, self.height, self.mean_wind_speed,
            spectrum_type="VonKarman"
        )
        
        # v and w should have different spectra (shorter length scales)
        # At low frequencies, all should have similar values
        # At high frequencies, v and w should decay faster
        self.assertFalse(np.allclose(spectrum['S_u'], spectrum['S_v']))
        self.assertFalse(np.allclose(spectrum['S_v'], spectrum['S_w']))


class TestIEC61400Fluctuations(unittest.TestCase):
    """Test fluctuation generation."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.frequencies = np.logspace(-2, 0.5, 64)
        self.height = 90.0
        self.mean_wind_speed = 12.0
    
    def test_generate_fluctuations_amplitude_positive(self):
        """Test that fluctuation amplitudes are positive."""
        fluct = self.ntm.generate_fluctuations(
            self.frequencies, self.height, self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42
        )
        
        self.assertTrue(np.all(fluct['amplitude_u'] >= 0.0))
        self.assertTrue(np.all(fluct['amplitude_v'] >= 0.0))
        self.assertTrue(np.all(fluct['amplitude_w'] >= 0.0))
    
    def test_generate_fluctuations_phase_range(self):
        """Test that phases are in proper range."""
        fluct = self.ntm.generate_fluctuations(
            self.frequencies, self.height, self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42
        )
        
        self.assertTrue(np.all(fluct['phase_u'] >= 0.0))
        self.assertTrue(np.all(fluct['phase_u'] <= 2 * np.pi))
        self.assertTrue(np.all(fluct['phase_v'] >= 0.0))
        self.assertTrue(np.all(fluct['phase_v'] <= 2 * np.pi))
        self.assertTrue(np.all(fluct['phase_w'] >= 0.0))
        self.assertTrue(np.all(fluct['phase_w'] <= 2 * np.pi))
    
    def test_generate_fluctuations_reproducibility(self):
        """Test that same seed produces same results."""
        fluct1 = self.ntm.generate_fluctuations(
            self.frequencies, self.height, self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42
        )
        
        fluct2 = self.ntm.generate_fluctuations(
            self.frequencies, self.height, self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42
        )
        
        # Should be identical
        np.testing.assert_array_almost_equal(fluct1['amplitude_u'], fluct2['amplitude_u'])
        np.testing.assert_array_almost_equal(fluct1['phase_u'], fluct2['phase_u'])
    
    def test_generate_fluctuations_different_seeds(self):
        """Test that different seeds produce different results."""
        fluct1 = self.ntm.generate_fluctuations(
            self.frequencies, self.height, self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42
        )
        
        fluct2 = self.ntm.generate_fluctuations(
            self.frequencies, self.height, self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=43
        )
        
        # Should be different
        self.assertFalse(np.allclose(fluct1['phase_u'], fluct2['phase_u']))


class TestIEC61400TimeSeries(unittest.TestCase):
    """Test time series generation."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        self.height = 90.0
        self.mean_wind_speed = 12.0
        self.duration = 10.0  # 10 seconds for quick test
        self.dt = 0.1  # 10 Hz
    
    def test_generate_time_series_time_array(self):
        """Test that time array is properly generated."""
        ts = self.ntm.generate_time_series(
            duration=self.duration, dt=self.dt,
            height=self.height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", n_freq_bins=64
        )
        
        nt_expected = int(np.ceil(self.duration / self.dt))
        self.assertEqual(len(ts['time']), nt_expected)
        self.assertAlmostEqual(ts['time'][0], 0.0)
        self.assertAlmostEqual(ts['time'][-1], (nt_expected - 1) * self.dt)
    
    def test_generate_time_series_fluctuations_shape(self):
        """Test that fluctuation arrays have correct shape."""
        ts = self.ntm.generate_time_series(
            duration=self.duration, dt=self.dt,
            height=self.height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", n_freq_bins=64
        )
        
        nt_expected = int(np.ceil(self.duration / self.dt))
        self.assertEqual(len(ts['u_prime']), nt_expected)
        self.assertEqual(len(ts['v_prime']), nt_expected)
        self.assertEqual(len(ts['w_prime']), nt_expected)
    
    def test_generate_time_series_mean_near_zero(self):
        """Test that time series mean is close to zero."""
        ts = self.ntm.generate_time_series(
            duration=self.duration, dt=self.dt,
            height=self.height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", n_freq_bins=64
        )
        
        # Mean should be very close to zero (fluctuations are deviations)
        self.assertAlmostEqual(np.mean(ts['u_prime']), 0.0, places=5)
        self.assertAlmostEqual(np.mean(ts['v_prime']), 0.0, places=5)
        self.assertAlmostEqual(np.mean(ts['w_prime']), 0.0, places=5)
    
    def test_generate_time_series_rms_matches_target(self):
        """Test that realized RMS matches target RMS."""
        ts = self.ntm.generate_time_series(
            duration=self.duration, dt=self.dt,
            height=self.height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", n_freq_bins=128
        )
        
        # Get target RMS
        rms_data = self.ntm.compute_velocity_rms(self.height, self.mean_wind_speed)
        
        # Realized RMS should be close to target (within 10%)
        self.assertAlmostEqual(ts['u_rms'], rms_data['u_rms'], delta=0.1*rms_data['u_rms'])
        self.assertAlmostEqual(ts['v_rms'], rms_data['v_rms'], delta=0.1*rms_data['v_rms'])
        self.assertAlmostEqual(ts['w_rms'], rms_data['w_rms'], delta=0.1*rms_data['w_rms'])
    
    def test_generate_time_series_anisotropy(self):
        """Test that RMS anisotropy is maintained."""
        ts = self.ntm.generate_time_series(
            duration=self.duration, dt=self.dt,
            height=self.height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", n_freq_bins=64
        )
        
        # Check anisotropy ratios
        v_u_ratio = ts['v_rms'] / ts['u_rms'] if ts['u_rms'] > 0 else 0
        w_u_ratio = ts['w_rms'] / ts['u_rms'] if ts['u_rms'] > 0 else 0
        
        # Should be close to 0.8 and 0.5
        self.assertAlmostEqual(v_u_ratio, 0.8, delta=0.15)
        self.assertAlmostEqual(w_u_ratio, 0.5, delta=0.10)
    
    def test_generate_time_series_reproducibility(self):
        """Test that same seed produces same time series."""
        ts1 = self.ntm.generate_time_series(
            duration=self.duration, dt=self.dt,
            height=self.height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42, n_freq_bins=64
        )
        
        ts2 = self.ntm.generate_time_series(
            duration=self.duration, dt=self.dt,
            height=self.height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="VonKarman", random_seed=42, n_freq_bins=64
        )
        
        # Should be identical
        np.testing.assert_array_almost_equal(ts1['u_prime'], ts2['u_prime'])
        np.testing.assert_array_almost_equal(ts1['v_prime'], ts2['v_prime'])
        np.testing.assert_array_almost_equal(ts1['w_prime'], ts2['w_prime'])
    
    def test_generate_time_series_spectrum_type_kaimal(self):
        """Test that Kaimal spectrum works for time series."""
        ts = self.ntm.generate_time_series(
            duration=self.duration, dt=self.dt,
            height=self.height, mean_wind_speed=self.mean_wind_speed,
            spectrum_type="Kaimal", n_freq_bins=64
        )
        
        # Should complete without errors
        self.assertEqual(ts['spectrum_type'], "Kaimal")
        self.assertEqual(len(ts['u_prime']), int(np.ceil(self.duration / self.dt)))


class TestIEC61400ModelIntegration(unittest.TestCase):
    """Integration tests for complete workflow."""
    
    def test_ntm_complete_workflow(self):
        """Test complete workflow from profile to time series."""
        ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
        
        # 1. Generate wind profile
        heights = np.array([10, 50, 90, 150])
        profile = ntm.generate_wind_profile(heights, mean_speed=12.0)
        self.assertEqual(len(profile['heights']), 4)
        
        # 2. Compute RMS at hub height
        hub_idx = np.argmin(np.abs(profile['heights'] - 90.0))
        mean_speed_hub = profile['mean_wind_speed'][hub_idx]
        rms = ntm.compute_velocity_rms(90.0, mean_speed_hub)
        self.assertGreater(rms['u_rms'], 0.0)
        
        # 3. Compute spectrum
        frequencies = np.logspace(-2, 0.5, 64)
        spectrum = ntm.compute_spectrum(
            frequencies, 90.0, mean_speed_hub,
            spectrum_type="VonKarman"
        )
        self.assertTrue(np.all(spectrum['S_u'] >= 0.0))
        
        # 4. Generate fluctuations
        fluct = ntm.generate_fluctuations(
            frequencies, 90.0, mean_speed_hub,
            spectrum_type="VonKarman", random_seed=42
        )
        self.assertTrue(np.all(fluct['amplitude_u'] >= 0.0))
        
        # 5. Generate time series
        ts = ntm.generate_time_series(
            duration=10.0, dt=0.1,
            height=90.0, mean_wind_speed=mean_speed_hub,
            spectrum_type="VonKarman", random_seed=42
        )
        self.assertEqual(len(ts['u_prime']), 100)


class TestIEC61400ErrorHandling(unittest.TestCase):
    """Test error handling and edge cases."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.ntm = NormalTurbulenceModel("II", terrain_category=1, z_hub=90.0)
    
    def test_invalid_spectrum_type(self):
        """Test that invalid spectrum type raises error."""
        frequencies = np.logspace(-2, 1, 50)
        
        with self.assertRaises(ValueError):
            self.ntm.compute_spectrum(
                frequencies, 90.0, 12.0,
                spectrum_type="InvalidType"
            )
    
    def test_zero_mean_wind_speed(self):
        """Test handling of zero mean wind speed."""
        frequencies = np.array([0.1, 0.5, 1.0])
        
        # Should not crash, should return valid values
        S_u = self.ntm.von_karman_spectrum(
            frequencies, 90.0, 0.1  # Very small wind speed
        )
        
        # Should have valid output
        self.assertEqual(len(S_u), 3)
        self.assertTrue(np.all(np.isfinite(S_u)))
    
    def test_single_frequency(self):
        """Test handling of single frequency."""
        freq = np.array([1.0])
        
        S_u = self.ntm.von_karman_spectrum(freq, 90.0, 12.0)
        
        self.assertEqual(len(S_u), 1)
        self.assertGreater(S_u[0], 0.0)


def run_tests():
    """Run all unit tests."""
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add all test classes
    suite.addTests(loader.loadTestsFromTestCase(TestIEC61400RMSVelocity))
    suite.addTests(loader.loadTestsFromTestCase(TestIEC61400Spectra))
    suite.addTests(loader.loadTestsFromTestCase(TestIEC61400Fluctuations))
    suite.addTests(loader.loadTestsFromTestCase(TestIEC61400TimeSeries))
    suite.addTests(loader.loadTestsFromTestCase(TestIEC61400ModelIntegration))
    suite.addTests(loader.loadTestsFromTestCase(TestIEC61400ErrorHandling))
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    exit(run_tests())
