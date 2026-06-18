#!/usr/bin/env python3
"""
test_layout_optimization.py - Unit tests for wind farm layout optimization

Tests coverage:
- Wind field caching (HDF5 save/load)
- Wind field interpolation (trilinear)
- Wake loss calculations (Bastankhah model)
- Layout optimization (constraints, objective function)
- Integration tests
"""

import unittest
import tempfile
import os
import numpy as np
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from wind_field_cache import WindFieldCache
from wake_models import BastankhahWakeModel, WakeLossCalculator, calculate_power_output
from layout_optimizer import WindFarmLayoutOptimizer


class TestWindFieldCache(unittest.TestCase):
    """Test wind field caching and serialization."""
    
    def setUp(self):
        """Create synthetic wind field for testing."""
        self.nx, self.ny, self.nz = 20, 20, 10
        self.dx, self.dy, self.dz = 100.0, 100.0, 10.0
        
        self.cache = WindFieldCache()
        self.cache.u_field = np.ones((self.nz, self.ny, self.nx)) * 10.0
        self.cache.v_field = np.zeros((self.nz, self.ny, self.nx))
        self.cache.w_field = np.zeros((self.nz, self.ny, self.nx))
        self.cache.terrain = np.zeros((self.ny, self.nx))
        
        self.cache.nx = self.nx
        self.cache.ny = self.ny
        self.cache.nz = self.nz
        self.cache.dx = self.dx
        self.cache.dy = self.dy
        self.cache.dz = self.dz
        self.cache.xmin = 0.0
        self.cache.ymin = 0.0
        self.cache.zmin = 0.0
        
        self.cache.grid_x = np.linspace(0, (self.nx-1)*self.dx, self.nx)
        self.cache.grid_y = np.linspace(0, (self.ny-1)*self.dy, self.ny)
        self.cache.grid_z = np.linspace(0, (self.nz-1)*self.dz, self.nz)
    
    def test_domain_bounds(self):
        """Test domain boundary calculation."""
        bounds = self.cache.get_domain_bounds()
        
        self.assertEqual(bounds['xmin'], 0.0)
        self.assertEqual(bounds['xmax'], (self.nx-1)*self.dx)
        self.assertEqual(bounds['ymin'], 0.0)
        self.assertEqual(bounds['ymax'], (self.ny-1)*self.dy)
    
    def test_point_in_domain(self):
        """Test domain point checking."""
        # Point inside domain
        self.assertTrue(self.cache.is_point_in_domain(500, 500, 50))
        
        # Point outside domain (x)
        self.assertFalse(self.cache.is_point_in_domain(5000, 500, 50))
        
        # Point outside domain (y)
        self.assertFalse(self.cache.is_point_in_domain(500, 5000, 50))
    
    def test_trilinear_interpolation(self):
        """Test trilinear velocity interpolation."""
        # Point at grid node should return exact value
        u, v, w = self.cache.interpolate_velocity_trilinear(0, 0, 0)
        
        self.assertAlmostEqual(u, 10.0, places=5)
        self.assertAlmostEqual(v, 0.0, places=5)
        self.assertAlmostEqual(w, 0.0, places=5)
    
    def test_trilinear_interpolation_midpoint(self):
        """Test trilinear interpolation at grid midpoint."""
        # At midpoint between nodes, should interpolate
        u, v, w = self.cache.interpolate_velocity_trilinear(50, 50, 5)
        
        # Should be close to 10 m/s (uniform field)
        self.assertGreater(u, 9.9)
        self.assertLess(u, 10.1)
    
    def test_terrain_elevation(self):
        """Test terrain elevation interpolation."""
        z_terr = self.cache.get_terrain_elevation(500, 500)
        
        self.assertEqual(z_terr, 0.0)  # Flat terrain
    
    def test_wind_speed_and_direction(self):
        """Test wind speed and direction calculation."""
        # Uniform 10 m/s wind from west
        speed, direction = self.cache.get_wind_speed_and_direction(500, 500, 50)
        
        self.assertAlmostEqual(speed, 10.0, places=1)
        # Direction should be ~90 degrees (from west, meteorological convention)
        self.assertGreater(direction, 80)
        self.assertLess(direction, 100)
    
    def test_hdf5_save_load(self):
        """Test HDF5 serialization roundtrip."""
        with tempfile.NamedTemporaryFile(suffix='.h5', delete=False) as f:
            filename = f.name
        
        try:
            # Save
            self.cache.save(filename)
            self.assertTrue(os.path.exists(filename))
            
            # Load
            loaded = WindFieldCache.load(filename)
            
            # Verify data
            np.testing.assert_array_almost_equal(loaded.u_field, self.cache.u_field)
            self.assertEqual(loaded.nx, self.cache.nx)
            self.assertEqual(loaded.dy, self.cache.dy)
        
        finally:
            if os.path.exists(filename):
                os.remove(filename)


class TestBastankhahWakeModel(unittest.TestCase):
    """Test Bastankhah wake model."""
    
    def setUp(self):
        """Initialize wake model."""
        self.wake_model = BastankhahWakeModel(
            turbine_diameter=100.0,
            turbulence_intensity=0.10,
            ct=0.8
        )
    
    def test_no_deficit_upwind(self):
        """Test that upwind points have no deficit."""
        deficit = self.wake_model.calculate_wake_deficit(
            x_distance=-100,  # Upwind
            y_distance=0,
            freestream_speed=10.0
        )
        
        self.assertEqual(deficit, 0.0)
    
    def test_deficit_at_wake_center(self):
        """Test deficit at wake centerline."""
        deficit = self.wake_model.calculate_wake_deficit(
            x_distance=500,
            y_distance=0,  # Wake centerline
            freestream_speed=10.0
        )
        
        # Should have significant deficit
        self.assertGreater(deficit, 0.1)
        self.assertLess(deficit, 1.0)
    
    def test_deficit_outside_wake(self):
        """Test that far lateral points have minimal deficit."""
        deficit = self.wake_model.calculate_wake_deficit(
            x_distance=500,
            y_distance=1000,  # Far from centerline
            freestream_speed=10.0
        )
        
        # Should have minimal deficit
        self.assertLess(deficit, 0.05)
    
    def test_affected_speed(self):
        """Test affected wind speed calculation."""
        speed = self.wake_model.get_affected_speed(
            x_distance=500,
            y_distance=0,
            freestream_speed=10.0
        )
        
        # Should be less than freestream
        self.assertLess(speed, 10.0)
        self.assertGreater(speed, 0.0)


class TestWakeLossCalculator(unittest.TestCase):
    """Test multi-turbine wake loss calculations."""
    
    def setUp(self):
        """Initialize calculator."""
        self.calculator = WakeLossCalculator(
            turbine_diameter=100.0,
            turbulence_intensity=0.10,
            superposition_method='rss'
        )
    
    def test_no_upwind_turbines(self):
        """Test effective speed with no upwind turbines."""
        eff_speed = self.calculator.calculate_effective_wind_speed(
            target_x=1000, target_y=0, target_z=0,
            upwind_turbines=[],
            freestream_speed=10.0
        )
        
        self.assertEqual(eff_speed, 10.0)
    
    def test_single_upwind_turbine(self):
        """Test effective speed with one upwind turbine."""
        upwind = [{'id': 0, 'x': 500, 'y': 0, 'z': 0, 'speed': 10.0}]
        
        eff_speed = self.calculator.calculate_effective_wind_speed(
            target_x=1000, target_y=0, target_z=0,
            upwind_turbines=upwind,
            freestream_speed=10.0
        )
        
        # Should be reduced due to wake
        self.assertLess(eff_speed, 10.0)
        self.assertGreater(eff_speed, 0.0)
    
    def test_farm_wake_losses(self):
        """Test wake losses across entire farm."""
        layout = [
            {'id': 0, 'x': 0, 'y': 0, 'z': 0},
            {'id': 1, 'x': 500, 'y': 0, 'z': 0},
            {'id': 2, 'x': 1000, 'y': 0, 'z': 0},
        ]
        
        speeds = self.calculator.calculate_farm_wake_losses(layout, wind_speed=10.0)
        
        self.assertEqual(len(speeds), 3)
        
        # Upwind turbine should have full speed
        self.assertAlmostEqual(speeds[0], 10.0, places=1)
        
        # Downwind turbines should have reduced speed
        self.assertLess(speeds[1], 10.0)
        self.assertLess(speeds[2], speeds[1])  # Further downwind = worse


class TestLayoutOptimizer(unittest.TestCase):
    """Test layout optimization."""
    
    def setUp(self):
        """Create cache and optimizer for testing."""
        # Create simple wind field
        self.cache = WindFieldCache()
        self.cache.u_field = np.ones((10, 20, 20)) * 10.0
        self.cache.v_field = np.zeros((10, 20, 20))
        self.cache.w_field = np.zeros((10, 20, 20))
        self.cache.terrain = np.zeros((20, 20))
        
        self.cache.nx = 20
        self.cache.ny = 20
        self.cache.nz = 10
        self.cache.dx = 100.0
        self.cache.dy = 100.0
        self.cache.dz = 10.0
        self.cache.xmin = 0.0
        self.cache.ymin = 0.0
        self.cache.zmin = 0.0
        
        self.cache.grid_x = np.linspace(0, 1900, 20)
        self.cache.grid_y = np.linspace(0, 1900, 20)
        self.cache.grid_z = np.linspace(0, 90, 10)
        
        # Create optimizer
        layout = [
            {'id': 0, 'x': 500, 'y': 500, 'z': 0},
            {'id': 1, 'x': 1000, 'y': 500, 'z': 0},
        ]
        
        self.optimizer = WindFarmLayoutOptimizer(
            wind_cache=self.cache,
            turbines=layout,
            hub_height=90.0,
            rotor_diameter=100.0,
            min_spacing=300.0
        )
    
    def test_layout_vector_conversion(self):
        """Test layout <-> vector conversion."""
        layout = [
            {'id': 0, 'x': 100, 'y': 200, 'z': 0},
            {'id': 1, 'x': 300, 'y': 400, 'z': 0},
        ]
        
        # Convert to vector
        vector = self.optimizer._layout_to_vector(layout)
        
        np.testing.assert_array_almost_equal(vector, [100, 200, 300, 400])
        
        # Convert back
        layout2 = self.optimizer._vector_to_layout(vector)
        
        self.assertEqual(layout2[0]['x'], 100)
        self.assertEqual(layout2[1]['y'], 400)
    
    def test_spacing_constraint(self):
        """Test minimum spacing constraint."""
        # Valid spacing
        layout_valid = [
            {'id': 0, 'x': 100, 'y': 100, 'z': 0},
            {'id': 1, 'x': 500, 'y': 100, 'z': 0},
        ]
        
        self.assertTrue(self.optimizer._check_spacing_constraint(layout_valid))
        
        # Invalid spacing (too close)
        layout_invalid = [
            {'id': 0, 'x': 100, 'y': 100, 'z': 0},
            {'id': 1, 'x': 200, 'y': 100, 'z': 0},  # 100m < 300m min
        ]
        
        self.assertFalse(self.optimizer._check_spacing_constraint(layout_invalid))
    
    def test_domain_constraint(self):
        """Test domain boundary constraint."""
        # Inside domain
        layout_valid = [
            {'id': 0, 'x': 500, 'y': 500, 'z': 0},
        ]
        
        self.assertTrue(self.optimizer._check_domain_constraint(layout_valid))
        
        # Outside domain
        layout_invalid = [
            {'id': 0, 'x': 5000, 'y': 500, 'z': 0},
        ]
        
        self.assertFalse(self.optimizer._check_domain_constraint(layout_invalid))
    
    def test_layout_evaluation(self):
        """Test layout evaluation (AEP calculation)."""
        layout = [
            {'id': 0, 'x': 500, 'y': 500, 'z': 0},
            {'id': 1, 'x': 1000, 'y': 500, 'z': 0},
        ]
        
        aep, speeds = self.optimizer.evaluate_layout(layout)
        
        # Should have positive AEP
        self.assertGreater(aep, 0)
        
        # Should have effective speeds for both turbines
        self.assertIn(0, speeds)
        self.assertIn(1, speeds)
    
    def test_objective_function(self):
        """Test objective function (negative AEP)."""
        vector = np.array([500, 500, 1000, 500])
        
        obj_value = self.optimizer.objective_function(vector)
        
        # Should be negative (since we minimize -AEP)
        self.assertLess(obj_value, 0)


class TestPowerCalculation(unittest.TestCase):
    """Test power output calculations."""
    
    def test_power_zero_wind(self):
        """Test power at zero wind speed."""
        power = calculate_power_output(wind_speed=0.0)
        
        self.assertEqual(power, 0.0)
    
    def test_power_nonzero_wind(self):
        """Test power at nonzero wind speed."""
        power = calculate_power_output(wind_speed=10.0)
        
        # Should be positive
        self.assertGreater(power, 0.0)
    
    def test_power_cubic_relationship(self):
        """Test that power scales with wind speed cubed."""
        p1 = calculate_power_output(wind_speed=10.0)
        p2 = calculate_power_output(wind_speed=20.0)
        
        # Power should scale as speed^3, so 2x speed = 8x power
        ratio = p2 / p1
        
        self.assertAlmostEqual(ratio, 8.0, places=0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
