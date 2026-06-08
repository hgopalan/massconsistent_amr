#!/usr/bin/env python3
"""
test_aep_dispersion.py - Comprehensive tests for:
1. Annual Energy Production (AEP) Calculator
2. Fuga-style linearized wake lookup mapped onto AMReX mesh
3. Turbine wake-induced turbulence integration with Puff/LPDM dispersion
"""

import os
import sys
import shutil
import tempfile
import numpy as np
import subprocess
import unittest

# Ensure we can import modules from src/python
current_dir = os.path.dirname(os.path.abspath(__file__))
repo_root = current_dir
for _ in range(5):
    if os.path.exists(os.path.join(repo_root, "CMakeLists.txt")):
        break
    repo_root = os.path.dirname(repo_root)

SRC_PYTHON_DIR = os.path.join(repo_root, 'src', 'python')
sys.path.insert(0, SRC_PYTHON_DIR)

# Set PYTHONPATH for tests to load build/python
BUILD_PYTHON_DIR = os.path.join(repo_root, 'build', 'python')
sys.path.insert(0, BUILD_PYTHON_DIR)

try:
    from wind_solver import WindSolver
    from aep_calculator import AEPCalculator
    from fuga_lookup import FugaWakeLookup
    BINDINGS_AVAILABLE = True
except ImportError:
    BINDINGS_AVAILABLE = False


class TestAEPAndDispersion(unittest.TestCase):
    
    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        
        # Write flat terrain file
        self.terrain_file = os.path.join(self.test_dir, "terrain.csv")
        with open(self.terrain_file, "w") as f:
            f.write("# x, y, z\n")
            f.write("0.0, 0.0, 0.0\n")
            f.write("200.0, 0.0, 0.0\n")
            f.write("0.0, 200.0, 0.0\n")
            f.write("200.0, 200.0, 0.0\n")
            
        # Write turbines file
        self.turbines_file = os.path.join(self.test_dir, "turbines.csv")
        with open(self.turbines_file, "w") as f:
            f.write("# x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, power_curve_file\n")
            f.write("50.0, 100.0, 50.0, 40.0, 0.8, 0.0, 0.0, nrel_5mw.csv\n")
            f.write("150.0, 100.0, 50.0, 40.0, 0.8, 0.0, 0.0, nrel_5mw.csv\n")
            
        # Write power curve file
        self.power_curve_file = os.path.join(self.test_dir, "nrel_5mw.csv")
        with open(self.power_curve_file, "w") as f:
            f.write("# ws, power, ct\n")
            f.write("0.0, 0.0, 0.8\n")
            f.write("5.0, 1000.0, 0.8\n")
            f.write("10.0, 5000.0, 0.8\n")
            f.write("25.0, 5000.0, 0.8\n")
            
        # Write inputs.i for wind solver
        self.inputs_file = os.path.join(self.test_dir, "inputs.i")
        with open(self.inputs_file, "w") as f:
            f.write(f"terrain_file = {self.terrain_file}\n")
            f.write("enable_turbine_wake = true\n")
            f.write(f"turbine_file = {self.turbines_file}\n")
            f.write("turbine_wake_model_type = gaussian\n")
            f.write("turbine_wake_superposition = quadratic\n")
            f.write("U_ref = 10.0\n")
            f.write("V_ref = 0.0\n")
            f.write("z_ref = 50.0\n")
            f.write("z0 = 0.1\n")
            f.write("dx = 20.0\n")
            f.write("dy = 20.0\n")
            f.write("dz = 10.0\n")
            f.write("domain_height = 100.0\n")
            f.write("alpha_h = 1.0\n")
            f.write("alpha_v = 1.0\n")
            f.write("mlmg_verbose = 0\n")
            f.write("max_grid_size = 16\n")
            
    def tearDown(self):
        shutil.rmtree(self.test_dir)
        
    def test_aep_calculator(self):
        """Test AEP Calculator batch execution over a simple wind rose."""
        if not BINDINGS_AVAILABLE:
            self.skipTest("pyWindSolver bindings not available")
            
        calc = AEPCalculator(self.inputs_file)
        
        # Set up a small wind rose: 4 directions, 3 speeds
        wind_directions = [0.0, 90.0, 180.0, 270.0]
        wind_speeds = [5.0, 10.0, 15.0]
        probabilities = [
            [0.1, 0.1, 0.05],  # North
            [0.15, 0.15, 0.05], # East
            [0.05, 0.05, 0.05], # South
            [0.1, 0.1, 0.1]     # West
        ]
        
        res = calc.run_wind_rose(wind_speeds, wind_directions, probabilities)
        
        # Verify results
        results = res["results"]
        self.assertIn("total_aep_kwh", results)
        self.assertGreater(results["total_aep_kwh"], 0.0)
        self.assertEqual(len(results["sector_aep_kwh"]), 4)
        self.assertEqual(len(results["speed_aep_kwh"]), 3)
        self.assertEqual(len(results["turbine_aep_kwh"]), 2)
        
        # Verify profiling details
        profile = res["profile"]
        self.assertIn("avg_run_time_s", profile)
        self.assertIn("peak_memory_mb", profile)
        self.assertGreater(profile["grid_points"], 0)
        
    def test_fuga_lookup(self):
        """Test Fuga-style pre-computed lookup table generation and mesh mapping."""
        if not BINDINGS_AVAILABLE:
            self.skipTest("pyWindSolver bindings not available")
            
        fuga = FugaWakeLookup()
        
        # Query deficit at centerline, close downwind
        val = fuga.get_deficit(x_nd=5.0, y_nd=0.0, z_nd=0.0)
        self.assertGreater(val, 0.0)
        self.assertLess(val, 1.0)
        
        # Query far crosswind - should be close to 0
        val_far = fuga.get_deficit(x_nd=5.0, y_nd=3.5, z_nd=0.0)
        self.assertLess(val_far, val)
        
        # Test mapping lookups onto AMReX mesh
        solver = WindSolver(self.inputs_file)
        solver.solve()
        
        turbines_list = [
            {'x': 50.0, 'y': 100.0, 'hub_height': 50.0, 'rotor_diameter': 40.0},
            {'x': 150.0, 'y': 100.0, 'hub_height': 50.0, 'rotor_diameter': 40.0}
        ]
        
        mapped_vel = fuga.map_wakes_to_mesh_explicit(solver, turbines_list, superposition="quadratic")
        
        # Verify shape of output matches input
        self.assertEqual(mapped_vel['u'].shape, solver.get_velocity0()['u'].shape)
        self.assertEqual(mapped_vel['v'].shape, solver.get_velocity0()['v'].shape)
        
        # Verify deficit has been applied
        diff = solver.get_velocity0()['u'] - mapped_vel['u']
        self.assertTrue(np.any(diff > 0.0))
        
        solver.finalize()
        
    def test_puff_solver_turbine_dispersion(self):
        """Test puff_solver with integrated turbine wake-induced turbulence."""
        puff_inputs_file = os.path.join(self.test_dir, "puff_inputs.i")
        with open(puff_inputs_file, "w") as f:
            f.write("enable_puff = true\n")
            f.write("source_x = 50.0\n")
            f.write("source_y = 100.0\n")
            f.write("source_z = 50.0\n")
            f.write("emission_rate = 1.0\n")
            f.write("emission_duration = 50.0\n")
            f.write("K_h = 1.0\n")
            f.write("K_v = 0.5\n")
            f.write("dt_puff = 1.0\n")
            f.write("n_steps_puff = 10\n")
            f.write("output_freq_puff = 5\n")
            f.write("U_wind = 10.0\n")
            f.write("V_wind = 0.0\n")
            f.write("W_wind = 0.0\n")
            f.write("xmin = 0.0\n")
            f.write("xmax = 200.0\n")
            f.write("ymin = 0.0\n")
            f.write("ymax = 200.0\n")
            f.write("zmin = 0.0\n")
            f.write("zmax = 100.0\n")
            f.write("dx = 20.0\n")
            f.write("dy = 20.0\n")
            f.write("dz = 10.0\n")
            f.write(f"puff_output = {os.path.join(self.test_dir, 'puff_conc.csv')}\n")
            f.write(f"turbine_file = {self.turbines_file}\n")
            f.write("enable_turbine_wake_diffusivity = true\n")
            f.write("turbine_wake_model_type = gaussian\n")
            f.write("wake_added_turb_model = crespo_hernandez\n")
            f.write("turbine_wake_diffusivity_factor = 2.5\n")
            
        # Run puff_solver executable
        script_dir = os.path.dirname(os.path.abspath(__file__))
        repo_dir = script_dir
        for _ in range(5):
            if os.path.exists(os.path.join(repo_dir, "build", "puff_solver")):
                break
            repo_dir = os.path.dirname(repo_dir)
            
        exe_path = os.path.join(repo_dir, "build", "puff_solver")
        
        # Verify executable exists
        self.assertTrue(os.path.exists(exe_path), f"puff_solver executable not found at {exe_path}")
        
        cmd = [exe_path, puff_inputs_file]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        
        self.assertEqual(res.returncode, 0, f"puff_solver failed with error: {res.stderr}\nOutput: {res.stdout}")
        self.assertIn("Turbine Wake diffusivity: ENABLED", res.stdout)
        self.assertIn("Turbine file:", res.stdout)
        self.assertIn("Turbine wake model: gaussian", res.stdout)
        self.assertIn("Wake added turbulence model: crespo_hernandez", res.stdout)

    def test_puff_solver_adaptive_time_stepping(self):
        """Test puff_solver with adaptive (CFL-limited) time-stepping."""
        puff_inputs_file = os.path.join(self.test_dir, "puff_inputs_adaptive.i")
        with open(puff_inputs_file, "w") as f:
            f.write("enable_puff = true\n")
            f.write("source_x = 50.0\n")
            f.write("source_y = 100.0\n")
            f.write("source_z = 50.0\n")
            f.write("emission_rate = 1.0\n")
            f.write("emission_duration = 50.0\n")
            f.write("K_h = 1.0\n")
            f.write("K_v = 0.5\n")
            f.write("dt_puff = 1.0\n")
            f.write("n_steps_puff = 10\n")
            f.write("output_freq_puff = 5\n")
            f.write("enable_adaptive_time_stepping = true\n")
            f.write("cfl_limit = 0.2\n")
            f.write("U_wind = 40.0\n")
            f.write("V_wind = 0.0\n")
            f.write("W_wind = 0.0\n")
            f.write("xmin = 0.0\n")
            f.write("xmax = 200.0\n")
            f.write("ymin = 0.0\n")
            f.write("ymax = 200.0\n")
            f.write("zmin = 0.0\n")
            f.write("zmax = 100.0\n")
            f.write("dx = 10.0\n")
            f.write("dy = 10.0\n")
            f.write("dz = 10.0\n")
            f.write(f"puff_output = {os.path.join(self.test_dir, 'puff_conc_adaptive.csv')}\n")

        # Run puff_solver executable
        script_dir = os.path.dirname(os.path.abspath(__file__))
        repo_dir = script_dir
        for _ in range(5):
            if os.path.exists(os.path.join(repo_dir, "build", "puff_solver")):
                break
            repo_dir = os.path.dirname(repo_dir)
            
        exe_path = os.path.join(repo_dir, "build", "puff_solver")
        
        self.assertTrue(os.path.exists(exe_path), f"puff_solver executable not found at {exe_path}")
        
        cmd = [exe_path, puff_inputs_file]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        
        self.assertEqual(res.returncode, 0, f"puff_solver failed with error: {res.stderr}\nOutput: {res.stdout}")
        self.assertIn("Adaptive time-stepping: ENABLED", res.stdout)
        self.assertIn("dt_puff scaled from", res.stdout)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
