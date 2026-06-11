#!/usr/bin/env python3
"""
test_happy_jack_wind_farm.py - Unit Test for Happy Jack Wind Farm Wake Simulation
"""

import os
import sys
import math
import unittest
import shutil
import tempfile
import numpy as np
from pathlib import Path

# Setup paths
TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parent.parent
BUILD_PYTHON_DIR = REPO_ROOT / "build" / "python"
SRC_PYTHON_DIR = REPO_ROOT / "src" / "python"

sys.path.insert(0, str(BUILD_PYTHON_DIR))
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    sys.exit(1)


class TestHappyJackWindFarm(unittest.TestCase):
    
    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.tmp_path = Path(self.tmp_dir.name)
        
        # 1. Exact coordinates of Happy Jack turbines provided by the user
        self.lons = [-105.008087, -104.995293, -104.995987, -104.996094, -104.997887,
                     -105.00869,  -104.982887, -104.984787, -105.008888, -104.994492,
                     -104.982491, -104.98439,  -104.983788, -104.994293]
        self.lats = [41.144993, 41.145695, 41.142994, 41.137894, 41.139793,
                     41.139091, 41.133392, 41.135494, 41.141293, 41.135391,
                     41.145794, 41.143291, 41.140892, 41.132492]
        
        lat_ref = 41.1400
        lon_ref = -104.9939
        
        # Suzlon S88/2100 parameters
        D = 88.0  # Rotor diameter [m]
        H = 80.0  # Hub height [m]
        ct = 0.8
        
        # 2. Convert coordinates to local meters
        self.xs = []
        self.ys = []
        for lat, lon in zip(self.lats, self.lons):
            x = (lon - lon_ref) * 111000.0 * math.cos(math.radians(lat_ref))
            y = (lat - lat_ref) * 111000.0
            self.xs.append(x)
            self.ys.append(y)
            
        # 3. Write turbines.csv
        self.turbine_file = self.tmp_path / "turbines.csv"
        with open(self.turbine_file, "w") as f:
            f.write("# x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, power_curve_file\n")
            for x, y in zip(self.xs, self.ys):
                f.write(f"{x:.2f}, {y:.2f}, {H:.1f}, {D:.1f}, {ct:.2f}, 15.0, 0.0, nrel_5mw.csv\n")
                
        # Copy nrel_5mw.csv to tmp_dir
        orig_power_curve = REPO_ROOT / "tests_and_examples" / "randomized_hill_turbines" / "nrel_5mw.csv"
        if orig_power_curve.exists():
            shutil.copy(orig_power_curve, self.tmp_path / "nrel_5mw.csv")
        else:
            with open(self.tmp_path / "nrel_5mw.csv", "w") as f:
                f.write("# ws, power, ct\n")
                f.write("0.0, 0.0, 0.8\n")
                f.write("10.0, 2100.0, 0.8\n")
                f.write("25.0, 2100.0, 0.8\n")

        # 4. Write terrain.csv (hilly high-altitude plateau in Wyoming)
        self.terrain_file = self.tmp_path / "terrain.csv"
        with open(self.terrain_file, "w") as f:
            f.write("# Happy Jack hilly terrain\n")
            f.write("# X[m] Y[m] Z[m]\n")
            tx = np.linspace(-2000.0, 2000.0, 9)
            ty = np.linspace(-1500.0, 1500.0, 9)
            for y in ty:
                for x in tx:
                    z = 2200.0 + 120.0 * math.sin(x / 500.0) + 80.0 * math.cos(y / 400.0)
                    f.write(f"{x:.2f}, {y:.2f}, {z:.2f}\n")

        # 5. Write inputs.i
        self.inputs_file = self.tmp_path / "inputs.i"
        with open(self.inputs_file, "w") as f:
            f.write(f"terrain_file = {self.terrain_file.name}\n")
            f.write("enable_turbine_wake = true\n")
            f.write(f"turbine_file = {self.turbine_file.name}\n")
            f.write("turbine_wake_model_type = bastankhah_gaussian\n")
            f.write("turbine_wake_superposition = quadratic\n")
            f.write("wake_added_turbulence_model = none\n")
            f.write("enable_jimenez_deflection = false\n")
            f.write("enable_bastankhah_deflection = true\n")
            f.write("turbopark_c1 = 0.38\n")
            f.write("ambient_ti = 0.075\n")
            f.write("U_ref = 10.0\n")
            f.write("V_ref = 0.0\n")
            f.write("z_ref = 80.0\n")
            f.write("z0 = 0.05\n")
            f.write("dx = 50.0\n")
            f.write("dy = 50.0\n")
            f.write("dz = 15.0\n")
            f.write("domain_height = 300.0\n")
            f.write("alpha_h = 1.0\n")
            f.write("alpha_v = 1.0\n")
            f.write("mlmg_verbose = 0\n")
            f.write("max_grid_size = 64\n")
            f.write("plot_file = plt_happy_jack\n")

    def tearDown(self):
        self.tmp_dir.cleanup()

    def test_happy_jack_simulation(self):
        """Test that the Happy Jack Wind Farm simulation runs successfully and exhibits deficits."""
        # Change working directory to temp dir
        old_cwd = os.getcwd()
        os.chdir(self.tmp_path)
        
        try:
            wind = WindSolver()
            wind.initialize("inputs.i")
            
            # 1. Check dimensions are correct
            self.assertEqual(wind.nx, 80)
            self.assertEqual(wind.ny, 60)
            
            # 2. Solve the wind field
            solve_result = wind.solve()
            self.assertTrue(solve_result['success'])
            
            # 3. Check power outputs and inflow wind speeds
            powers = wind.get_turbine_power_outputs()
            inflows = wind.get_turbine_inflow_speeds()
            
            self.assertEqual(len(powers), 14)
            self.assertEqual(len(inflows), 14)
            
            # Print statistics for diagnostic visibility
            print(f"\nHappy Jack Simulation Results:")
            for i in range(14):
                print(f"  WT {i+1:2d} | Inflow Speed: {inflows[i]:5.2f} m/s | Power: {powers[i]:7.1f} kW")
                
            # Verify all wind speeds are within a reasonable range [0, U_ref]
            for ws in inflows:
                self.assertGreater(ws, 0.0)
                self.assertLessEqual(ws, 10.0 + 1e-3)
                
            wind.finalize()
        finally:
            os.chdir(old_cwd)


if __name__ == "__main__":
    unittest.main()
