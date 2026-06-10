#!/usr/bin/env python3
"""
test_reactive_transport.py - Unit tests for PHREEQC coupling infrastructure

Tests field extraction, NetCDF I/O, and PHREEQC input generation.
"""

import unittest
import tempfile
from pathlib import Path
import numpy as np
from unittest.mock import Mock, MagicMock

from geochemical_coupling import FieldExtractor, AtmosphericField, StabilityClass
from phreeqc_utils import PHREEQCGenerator, BoundaryCondition
from netcdf_io import NetCDFHandler, ASCIIExporter


class MockWindSolver:
    """Mock wind solver for testing."""
    
    def __init__(self):
        self.initialized = True
        self.solved = True
        self.nx, self.ny, self.nz = 10, 10, 10
        self.xmin, self.xmax = 0, 1000
        self.ymin, self.ymax = 0, 1000
        self.zmin, self.zmax = 0, 1000
        self.dx = (self.xmax - self.xmin) / self.nx
        self.dy = (self.ymax - self.ymin) / self.ny
        self.dz = (self.zmax - self.zmin) / self.nz
    
    def get_velocity(self):
        """Return mock velocity fields."""
        u = np.random.randn(self.nz, self.ny, self.nx) * 2 + 5
        v = np.random.randn(self.nz, self.ny, self.nx) * 2
        w = np.random.randn(self.nz, self.ny, self.nx) * 0.1
        return {'u': u, 'v': v, 'w': w}
    
    def get_terrain(self):
        """Return mock terrain."""
        return np.random.rand(self.ny, self.nx) * 100
    
    def get_velocity0(self):
        """Return mock initial velocity."""
        return self.get_velocity()
    
    def get_lambda(self):
        """Return mock Lagrange multiplier."""
        return np.random.randn(self.nz, self.ny, self.nx)
    
    def get_div0(self):
        """Return mock divergence."""
        return np.random.randn(self.nz, self.ny, self.nx) * 1e-6


class TestFieldExtractor(unittest.TestCase):
    """Test FieldExtractor functionality."""
    
    def setUp(self):
        self.wind_solver = MockWindSolver()
        self.extractor = FieldExtractor(self.wind_solver)
    
    def test_initialization(self):
        """Test FieldExtractor initialization."""
        self.assertIsNotNone(self.extractor)
        self.assertTrue(self.extractor.wind_solver.initialized)
    
    def test_extract_all_fields(self):
        """Test extraction of all atmospheric fields."""
        fields = self.extractor.extract_all_fields()
        
        self.assertIsInstance(fields, AtmosphericField)
        self.assertEqual(fields.u.shape, (self.wind_solver.nz, self.wind_solver.ny, self.wind_solver.nx))
        self.assertEqual(fields.T.shape, (self.wind_solver.nz, self.wind_solver.ny, self.wind_solver.nx))
        self.assertIsNotNone(fields.terrain)
        self.assertIsNotNone(fields.u_star)
    
    def test_export_velocity_magnitude(self):
        """Test velocity magnitude export."""
        fields = self.extractor.extract_all_fields()
        u_mag = self.extractor.export_velocity_magnitude(fields, z_level=None)
        
        self.assertEqual(u_mag.shape, (self.wind_solver.ny, self.wind_solver.nx))
        self.assertTrue(np.all(u_mag >= 0))
    
    def test_export_temperature_profile(self):
        """Test temperature profile export."""
        fields = self.extractor.extract_all_fields()
        z_agl, T_profile = self.extractor.export_temperature_profile(fields)
        
        self.assertEqual(len(z_agl), self.wind_solver.nz)
        self.assertEqual(len(T_profile), self.wind_solver.nz)
        self.assertTrue(np.all(T_profile > 0))  # Temperature in Kelvin
    
    def test_export_dispersivity(self):
        """Test dispersivity export."""
        fields = self.extractor.extract_all_fields()
        alpha_h, alpha_v = self.extractor.export_dispersivity(fields, z_level=500)
        
        self.assertEqual(alpha_h.shape, (self.wind_solver.ny, self.wind_solver.nx))
        self.assertEqual(alpha_v.shape, (self.wind_solver.ny, self.wind_solver.nx))
        self.assertTrue(np.all(alpha_h >= 0))
        self.assertTrue(np.all(alpha_v >= 0))
    
    def test_export_stability_rate_factor(self):
        """Test stability-based rate factor export."""
        fields = self.extractor.extract_all_fields()
        rate_factor = self.extractor.export_stability_rate_factor(fields)
        
        self.assertEqual(rate_factor.shape, (self.wind_solver.ny, self.wind_solver.nx))
        self.assertTrue(np.all(rate_factor > 0.4))  # Should be > 0.5 (min for F class)
        self.assertTrue(np.all(rate_factor < 1.6))  # Should be < 1.5 (max for A class)
    
    def test_export_oxygen_delivery_rate(self):
        """Test oxygen delivery rate export."""
        fields = self.extractor.extract_all_fields()
        O2_factor = self.extractor.export_oxygen_delivery_rate(fields)
        
        self.assertEqual(O2_factor.shape, (self.wind_solver.ny, self.wind_solver.nx))
        self.assertTrue(np.all(O2_factor > 0))
    
    def test_export_co2_fugacity(self):
        """Test CO₂ fugacity export."""
        fields = self.extractor.extract_all_fields()
        P_co2 = self.extractor.export_co2_fugacity(fields)
        
        self.assertEqual(P_co2.shape, (self.wind_solver.ny, self.wind_solver.nx))
        self.assertTrue(np.all(P_co2 > 0))
        self.assertTrue(np.all(P_co2 < 50000))  # Reasonable pressure range


class TestPHREEQCGenerator(unittest.TestCase):
    """Test PHREEQC input file generation."""
    
    def setUp(self):
        self.generator = PHREEQCGenerator()
        self.temp_dir = tempfile.TemporaryDirectory()
    
    def tearDown(self):
        self.temp_dir.cleanup()
    
    def test_generate_amd_simulation(self):
        """Test AMD input file generation."""
        output_file = Path(self.temp_dir.name) / "amd_test.dat"
        
        bcs = {
            'temperature': BoundaryCondition('temperature', 'float', 20.0, units='C'),
            'O2_concentration': BoundaryCondition('O2', 'float', 240.0, units='umol/kgw'),
            'pe': BoundaryCondition('pe', 'float', 12.0, units=''),
        }
        
        result = self.generator.generate_amd_simulation(str(output_file), bcs)
        
        self.assertTrue(Path(result).exists())
        with open(result, 'r') as f:
            content = f.read()
            self.assertIn('SOLUTION', content)
            self.assertIn('KINETICS', content)
            self.assertIn('Pyrite', content)
    
    def test_generate_leaching_simulation(self):
        """Test leaching input file generation."""
        output_file = Path(self.temp_dir.name) / "leaching_test.dat"
        
        bcs = {
            'temperature': BoundaryCondition('temperature', 'float', 25.0, units='C'),
            'co2_fugacity': BoundaryCondition('CO2', 'float', 0.0004, units='atm'),
        }
        
        result = self.generator.generate_leaching_simulation(str(output_file), bcs)
        
        self.assertTrue(Path(result).exists())
        with open(result, 'r') as f:
            content = f.read()
            self.assertIn('SOLUTION', content)
            self.assertIn('KINETICS', content)
    
    def test_validate_phreeqc_input(self):
        """Test PHREEQC input validation."""
        # Create a valid test file
        output_file = Path(self.temp_dir.name) / "test_valid.dat"
        with open(output_file, 'w') as f:
            f.write("SOLUTION 0\n pH 7\n END\n")
        
        is_valid, errors = self.generator.validate_phreeqc_input(str(output_file))
        self.assertTrue(is_valid)
        self.assertEqual(len(errors), 0)
        
        # Create an invalid test file
        output_file = Path(self.temp_dir.name) / "test_invalid.dat"
        with open(output_file, 'w') as f:
            f.write("SOLUTION 0\n pH 7\n")  # Missing END
        
        is_valid, errors = self.generator.validate_phreeqc_input(str(output_file))
        self.assertFalse(is_valid)
        self.assertGreater(len(errors), 0)


class TestNetCDFHandler(unittest.TestCase):
    """Test NetCDF I/O operations."""
    
    def setUp(self):
        self.handler = NetCDFHandler(check_netcdf=False)
        self.temp_dir = tempfile.TemporaryDirectory()
        
        # Create mock fields
        nz, ny, nx = 5, 5, 5
        self.fields = AtmosphericField(
            u=np.random.randn(nz, ny, nx),
            v=np.random.randn(nz, ny, nx),
            w=np.random.randn(nz, ny, nx) * 0.1,
            T=np.full((nz, ny, nx), 288.15),
            RH=np.full((nz, ny, nx), 65.0),
            P=np.full((nz, ny, nx), 101325.0),
            K_h=np.full((nz, ny, nx), 1.0),
            K_v=np.full((nz, ny, nx), 0.1),
            u_star=np.full((ny, nx), 0.3),
            stability_class=np.full((ny, nx), 3, dtype=int),
            z_inv=np.full((ny, nx), 1000.0),
            terrain=np.random.rand(ny, nx) * 100,
            coord_x=np.linspace(0, 1000, nx),
            coord_y=np.linspace(0, 1000, ny),
            coord_z=np.linspace(0, 1000, nz),
        )
    
    def tearDown(self):
        self.temp_dir.cleanup()
    
    def test_ascii_export_temperature(self):
        """Test ASCII temperature profile export."""
        output_file = Path(self.temp_dir.name) / "temp.dat"
        ASCIIExporter.export_temperature_profile(self.fields, str(output_file))
        
        self.assertTrue(output_file.exists())
        with open(output_file, 'r') as f:
            lines = f.readlines()
            self.assertGreater(len(lines), 3)  # Header + data
    
    def test_ascii_export_wind(self):
        """Test ASCII wind field export."""
        output_file = Path(self.temp_dir.name) / "wind.dat"
        ASCIIExporter.export_wind_field(self.fields, str(output_file))
        
        self.assertTrue(output_file.exists())
        with open(output_file, 'r') as f:
            lines = f.readlines()
            self.assertGreater(len(lines), 3)


if __name__ == '__main__':
    unittest.main()
