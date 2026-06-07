#!/usr/bin/env python3
"""
test_pywake_coupling.py - Unit tests for PyWake export functions and turbine input parsing
"""

import os
import sys
import shutil
import tempfile
import numpy as np
import unittest

# Add src/python to path
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_PYTHON_DIR = os.path.join(os.path.dirname(TEST_DIR), 'src', 'python')
sys.path.insert(0, SRC_PYTHON_DIR)

from wind_solver import WindSolver
from pywake_coupling import MassConsistentSite, export_to_wasp_grd, to_wasp_grid_site, PYWAKE_AVAILABLE


class TestPyWakeCoupling(unittest.TestCase):
    
    def setUp(self):
        # Create a temporary directory for test files
        self.test_dir = tempfile.mkdtemp()
        
        # Write temporary terrain.csv (flat terrain)
        self.terrain_file = os.path.join(self.test_dir, "terrain.csv")
        with open(self.terrain_file, "w") as f:
            f.write("# Flat terrain\n")
            f.write("0.0   0.0   0.0\n")
            f.write("50.0  0.0   0.0\n")
            f.write("100.0 0.0   0.0\n")
            f.write("0.0   50.0  0.0\n")
            f.write("50.0  50.0  0.0\n")
            f.write("100.0 50.0  0.0\n")
            f.write("0.0   100.0 0.0\n")
            f.write("50.0  100.0 0.0\n")
            f.write("100.0 100.0 0.0\n")
            
        # Write temporary turbines.csv (including new yaw and orientation columns)
        # Columns: x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, power_curve_file
        self.turbines_file = os.path.join(self.test_dir, "turbines.csv")
        with open(self.turbines_file, "w") as f:
            f.write("# x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, power_curve_file\n")
            f.write("20.0, 50.0, 40.0, 30.0, 0.8, 15.0, 45.0, nrel_5mw.csv\n")
            f.write("80.0, 50.0, 40.0, 30.0, 0.8, 30.0, 90.0, nrel_5mw.csv\n")
            
        # Write temporary power curve file
        self.power_curve_file = os.path.join(self.test_dir, "nrel_5mw.csv")
        with open(self.power_curve_file, "w") as f:
            f.write("# ws, power, ct\n")
            f.write("0.0, 0.0, 0.8\n")
            f.write("10.0, 5000.0, 0.8\n")
            f.write("25.0, 5000.0, 0.8\n")
            
        # Write temporary inputs.i
        self.inputs_file = os.path.join(self.test_dir, "inputs.i")
        with open(self.inputs_file, "w") as f:
            f.write("terrain_file = {}\n".format(self.terrain_file))
            f.write("enable_turbine_wake = true\n")
            f.write("turbine_file = {}\n".format(self.turbines_file))
            f.write("turbine_wake_model_type = jensen\n")
            f.write("turbine_wake_superposition = quadratic\n")
            f.write("U_ref = 10.0\n")
            f.write("V_ref = 0.0\n")
            f.write("z_ref = 10.0\n")
            f.write("z0 = 0.1\n")
            f.write("dx = 10.0\n")
            f.write("dy = 10.0\n")
            f.write("dz = 10.0\n")
            f.write("domain_height = 100.0\n")
            f.write("alpha_h = 1.0\n")
            f.write("alpha_v = 1.0\n")
            f.write("mlmg_verbose = 0\n")
            f.write("max_grid_size = 32\n")
            f.write("plot_file = {}/plt_test\n".format(self.test_dir))
            
    def tearDown(self):
        # Clean up temporary directory
        shutil.rmtree(self.test_dir)
        
    def test_turbine_parsing(self):
        """Test C++ and Python parsing of new turbine yaw and orientation fields."""
        wind = WindSolver()
        wind.initialize(self.inputs_file)
        
        # Verify yaws and orientations are parsed correctly from turbines.csv
        yaws = wind.get_turbine_yaws()
        orientations = wind.get_turbine_orientations()
        
        self.assertEqual(len(yaws), 2)
        self.assertEqual(len(orientations), 2)
        
        # First turbine: yaw=15.0, orientation=45.0
        self.assertAlmostEqual(yaws[0], 15.0)
        self.assertAlmostEqual(orientations[0], 45.0)
        
        # Second turbine: yaw=30.0, orientation=90.0
        self.assertAlmostEqual(yaws[1], 30.0)
        self.assertAlmostEqual(orientations[1], 90.0)
        
        wind.finalize()

    def test_mass_consistent_site(self):
        """Test MassConsistentSite creation and local_wind extraction."""
        wind = WindSolver()
        wind.initialize(self.inputs_file)
        wind.solve()
        
        site = MassConsistentSite(wind)
        
        # Test terrain interpolation
        elev = site.get_terrain_elevation(50.0, 50.0)
        self.assertAlmostEqual(elev, 0.0)
        
        # Test local_wind extraction
        # We query a point (50.0, 50.0) at 40m AGL
        local_w = site.local_wind(50.0, 50.0, 40.0)
        
        self.assertEqual(len(local_w.x), 1)
        self.assertEqual(len(local_w.y), 1)
        self.assertEqual(len(local_w.h), 1)
        
        # WD_ilk and WS_ilk shapes should be (1, num_wd, num_ws)
        self.assertEqual(local_w.WD_ilk.shape[0], 1)
        self.assertEqual(local_w.WS_ilk.shape[0], 1)
        
        wind.finalize()

    def test_export_wasp_grd(self):
        """Test exporting 2D arrays to Surfer GRD format and WAsPGridSite."""
        wind = WindSolver()
        wind.initialize(self.inputs_file)
        wind.solve()
        
        output_dir = os.path.join(self.test_dir, "wasp_output")
        site = to_wasp_grid_site(wind, height_agl=40.0, output_dir=output_dir)
        
        # Check files are generated
        self.assertTrue(os.path.exists(os.path.join(output_dir, "elevation.grd")))
        self.assertTrue(os.path.exists(os.path.join(output_dir, "roughness.grd")))
        self.assertTrue(os.path.exists(os.path.join(output_dir, "wind_speed.grd")))
        self.assertTrue(os.path.exists(os.path.join(output_dir, "wind_direction.grd")))
        
        # Read elevation.grd and verify header
        with open(os.path.join(output_dir, "elevation.grd"), "r") as f:
            lines = [f.readline().strip() for _ in range(5)]
            
        self.assertEqual(lines[0], "DSAA")
        self.assertEqual(lines[1], f"{wind.nx} {wind.ny}") # nx ny
        self.assertEqual(lines[2], f"{wind.xmin:.1f} {wind.xmax:.1f}") # xmin xmax
        self.assertEqual(lines[3], f"{wind.ymin:.1f} {wind.ymax:.1f}") # ymin ymax
        
        # site should be either WAsPGridSite or MockWAsPGridSite
        self.assertIsNotNone(site)
        
        wind.finalize()


if __name__ == "__main__":
    unittest.main()
