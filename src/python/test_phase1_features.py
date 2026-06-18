#!/usr/bin/env python3
"""
test_phase1_features.py - Unit tests for Phase 1 features

Tests for:
- Feature 1: CSV Turbine Definition Format
- Feature 2: Wind Resource Summary Statistics
- Feature 3: Output Formatting for PyOptimization
"""

import os
import sys
import json
import csv
import tempfile
import unittest
import numpy as np

# Add src/python to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src', 'python'))

from turbine_io import TurbineLayout
from wind_resource_stats import WindResourceStats, compute_wind_rose_statistics
from pyoptimization_export import PyOptimizationExporter


class TestTurbineIO(unittest.TestCase):
    """Test turbine CSV I/O functionality."""
    
    def setUp(self):
        """Create temporary directory for test files."""
        self.temp_dir = tempfile.mkdtemp()
    
    def tearDown(self):
        """Clean up temporary files."""
        import shutil
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)
    
    def test_add_turbine(self):
        """Test adding turbines to layout."""
        layout = TurbineLayout()
        layout.add_turbine(0, 100.0, 200.0)
        layout.add_turbine(1, 500.0, 200.0, z_agl=50.0, hub_height=100.0)
        
        self.assertEqual(len(layout), 2)
        self.assertEqual(layout.turbines[0]['x'], 100.0)
        self.assertEqual(layout.turbines[1]['hub_height'], 100.0)
    
    def test_csv_write_read_roundtrip(self):
        """Test writing and reading CSV file."""
        # Create layout
        layout = TurbineLayout()
        layout.add_turbine(0, 100.0, 200.0, z_agl=0.0, hub_height=90.0)
        layout.add_turbine(1, 500.0, 200.0, z_agl=50.0, hub_height=100.0)
        layout.add_turbine(2, 1000.0, 300.0, turbine_type="DTU10MW")
        
        # Write to CSV
        csv_file = os.path.join(self.temp_dir, "turbines.csv")
        TurbineLayout.write_csv(layout, csv_file)
        
        # Verify file exists
        self.assertTrue(os.path.exists(csv_file))
        
        # Read back
        layout2 = TurbineLayout.read_csv(csv_file)
        
        # Verify data
        self.assertEqual(len(layout2), 3)
        self.assertEqual(layout2.turbines[0]['x'], 100.0)
        self.assertEqual(layout2.turbines[1]['z_agl'], 50.0)
        self.assertEqual(layout2.turbines[2]['turbine_type'], "DTU10MW")
    
    def test_layout_validator_spacing(self):
        """Test turbine spacing validation."""
        layout = TurbineLayout()
        layout.add_turbine(0, 100.0, 200.0)
        layout.add_turbine(1, 150.0, 200.0)  # 50m spacing - too close
        
        is_valid, errors = layout.validate_spacing(min_spacing=200.0)
        
        self.assertFalse(is_valid)
        self.assertEqual(len(errors), 1)
        self.assertIn("too close", errors[0])
    
    def test_layout_validator_domain_bounds(self):
        """Test domain bound validation."""
        layout = TurbineLayout()
        layout.domain_bounds = {'xmin': 0.0, 'xmax': 1000.0, 'ymin': 0.0, 'ymax': 1000.0}
        
        layout.add_turbine(0, 100.0, 200.0)  # Valid
        layout.add_turbine(1, 1500.0, 500.0)  # Outside domain
        
        is_valid, errors = layout.validate_spacing(min_spacing=300.0)
        
        self.assertFalse(is_valid)
        self.assertTrue(any("outside domain" in err for err in errors))


class TestWindResourceStats(unittest.TestCase):
    """Test wind resource statistics computation."""
    
    def test_compute_from_uniform_field(self):
        """Test statistics from uniform wind field."""
        u_field = np.full((10, 10), 10.0)  # Uniform 10 m/s east wind
        v_field = np.zeros((10, 10))  # No north component
        
        stats = WindResourceStats.compute_from_wind_field(u_field, v_field, height_agl=90.0)
        
        self.assertAlmostEqual(stats.mean_speed, 10.0, places=1)
        self.assertAlmostEqual(stats.std_speed, 0.0, places=1)
        self.assertEqual(stats.num_samples, 100)
        self.assertEqual(stats.height_agl, 90.0)
    
    def test_compute_from_variable_field(self):
        """Test statistics from variable wind field."""
        u_field = np.ones((10, 10)) * 10.0
        u_field[5:, :] = 20.0  # Half the domain has 20 m/s
        v_field = np.zeros((10, 10))
        
        stats = WindResourceStats.compute_from_wind_field(u_field, v_field)
        
        self.assertGreater(stats.std_speed, 0.0)
        self.assertTrue(10.0 < stats.mean_speed < 20.0)
    
    def test_direction_statistics(self):
        """Test wind direction statistics."""
        u_field = np.full((10, 10), 10.0)  # East wind
        v_field = np.full((10, 10), 10.0)  # North wind
        
        stats = WindResourceStats.compute_from_wind_field(u_field, v_field)
        
        # Wind from NE should be around 45 degrees
        self.assertTrue(30 < stats.mean_direction < 60)
    
    def test_stats_to_dict(self):
        """Test conversion to dictionary."""
        u_field = np.full((10, 10), 10.0)
        v_field = np.zeros((10, 10))
        
        stats = WindResourceStats.compute_from_wind_field(u_field, v_field, height_agl=100.0)
        stats_dict = stats.to_dict()
        
        self.assertIn('height_agl_m', stats_dict)
        self.assertIn('wind_speed', stats_dict)
        self.assertIn('weibull', stats_dict)
        self.assertEqual(stats_dict['height_agl_m'], 100.0)
    
    def test_stats_json_export(self):
        """Test JSON export of statistics."""
        u_field = np.full((10, 10), 10.0)
        v_field = np.zeros((10, 10))
        stats = WindResourceStats.compute_from_wind_field(u_field, v_field)
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            temp_file = f.name
        
        try:
            stats.to_json(temp_file)
            
            self.assertTrue(os.path.exists(temp_file))
            
            with open(temp_file, 'r') as f:
                data = json.load(f)
            
            self.assertIn('wind_speed', data)
            self.assertIn('mean_ms', data['wind_speed'])
        finally:
            if os.path.exists(temp_file):
                os.unlink(temp_file)
    
    def test_weibull_fitting(self):
        """Test Weibull parameter fitting."""
        # Create synthetic wind speed data
        rng = np.random.RandomState(42)
        speeds = rng.weibull(2.0, size=1000) * 10.0  # k=2, c~10
        
        k, c = WindResourceStats._fit_weibull(speeds)
        
        self.assertGreater(k, 0)
        self.assertGreater(c, 0)
        self.assertLess(k, 10)  # Reasonable shape parameter
        self.assertTrue(5 < c < 15)  # Reasonable scale parameter
    
    def test_wind_rose_statistics(self):
        """Test wind rose statistics computation."""
        speeds = np.array([5.0, 10.0, 15.0])
        directions = np.array([0.0, 90.0, 180.0, 270.0])
        
        # Uniform distribution
        probs = np.ones((4, 3)) / 12.0
        
        stats = compute_wind_rose_statistics(speeds, directions, probs)
        
        self.assertIn('mean_wind_speed_ms', stats)
        self.assertIn('mean_wind_direction_deg', stats)
        self.assertGreater(stats['mean_wind_speed_ms'], 0)


class TestPyOptimizationExporter(unittest.TestCase):
    """Test PyOptimization export functionality."""
    
    def setUp(self):
        """Create temporary directory for test files."""
        self.temp_dir = tempfile.mkdtemp()
    
    def tearDown(self):
        """Clean up temporary files."""
        import shutil
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)
    
    def test_add_turbine_result(self):
        """Test adding turbine results."""
        exporter = PyOptimizationExporter("test_farm")
        
        exporter.add_turbine_result(
            turbine_id=0,
            x=100.0,
            y=200.0,
            power_kw=4000.0,
            wind_speed_ms=10.0,
            wind_direction_deg=270.0
        )
        
        self.assertEqual(len(exporter.turbines), 1)
        self.assertEqual(exporter.turbines[0]['id'], 0)
        self.assertEqual(exporter.turbines[0]['power']['output_kw'], 4000.0)
    
    def test_set_farm_power(self):
        """Test setting farm power aggregates."""
        exporter = PyOptimizationExporter()
        exporter.set_farm_power(12000.0, annual_energy_gwh=40.0)
        
        self.assertEqual(exporter.farm_power_kw, 12000.0)
        self.assertEqual(exporter.farm_aep_gwh, 40.0)
    
    def test_set_wind_resource(self):
        """Test setting wind resource statistics."""
        exporter = PyOptimizationExporter()
        exporter.set_wind_resource(
            mean_speed_ms=9.5,
            mean_direction_deg=270.0,
            std_speed_ms=2.0,
            turbulence_intensity=0.08
        )
        
        self.assertIsNotNone(exporter.wind_resource)
        self.assertEqual(exporter.wind_resource['mean_speed_ms'], 9.5)
        self.assertEqual(exporter.wind_resource['turbulence_intensity'], 0.08)
    
    def test_to_dict(self):
        """Test conversion to dictionary."""
        exporter = PyOptimizationExporter("test_farm")
        exporter.add_turbine_result(
            turbine_id=0, x=100.0, y=200.0,
            power_kw=4000.0, wind_speed_ms=10.0, wind_direction_deg=270.0
        )
        exporter.set_farm_power(4000.0, annual_energy_gwh=35.0)
        exporter.set_wind_resource(mean_speed_ms=9.5, mean_direction_deg=270.0)
        
        result = exporter.to_dict()
        
        self.assertIn('farm_summary', result)
        self.assertIn('turbines', result)
        self.assertIn('wind_resource', result)
        self.assertEqual(result['farm_summary']['num_turbines'], 1)
        self.assertEqual(result['farm_summary']['annual_energy_gwh'], 35.0)
    
    def test_json_export(self):
        """Test JSON export."""
        exporter = PyOptimizationExporter("test_farm")
        exporter.add_turbine_result(
            turbine_id=0, x=100.0, y=200.0,
            power_kw=4000.0, wind_speed_ms=10.0, wind_direction_deg=270.0
        )
        exporter.set_farm_power(4000.0)
        
        json_file = os.path.join(self.temp_dir, "results.json")
        exporter.export_json(json_file)
        
        self.assertTrue(os.path.exists(json_file))
        
        with open(json_file, 'r') as f:
            data = json.load(f)
        
        self.assertEqual(data['farm_summary']['num_turbines'], 1)
    
    def test_csv_export_turbines(self):
        """Test CSV export of turbine results."""
        exporter = PyOptimizationExporter("test_farm")
        exporter.add_turbine_result(
            turbine_id=0, x=100.0, y=200.0,
            power_kw=4000.0, wind_speed_ms=10.0, wind_direction_deg=270.0
        )
        exporter.add_turbine_result(
            turbine_id=1, x=500.0, y=200.0,
            power_kw=3500.0, wind_speed_ms=9.5, wind_direction_deg=270.0
        )
        
        csv_file = os.path.join(self.temp_dir, "turbines.csv")
        exporter.export_turbine_csv(csv_file)
        
        self.assertTrue(os.path.exists(csv_file))
        
        with open(csv_file, 'r') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
        
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0]['power_kw'], '4000.0')
    
    def test_csv_export_summary(self):
        """Test CSV export of farm summary."""
        exporter = PyOptimizationExporter("test_farm")
        exporter.add_turbine_result(
            turbine_id=0, x=100.0, y=200.0,
            power_kw=4000.0, wind_speed_ms=10.0, wind_direction_deg=270.0
        )
        exporter.set_farm_power(4000.0, annual_energy_gwh=35.0)
        
        csv_file = os.path.join(self.temp_dir, "summary.csv")
        exporter.export_summary_csv(csv_file)
        
        self.assertTrue(os.path.exists(csv_file))
        
        with open(csv_file, 'r') as f:
            reader = csv.reader(f)
            rows = list(reader)
        
        self.assertGreater(len(rows), 0)


if __name__ == '__main__':
    unittest.main()
