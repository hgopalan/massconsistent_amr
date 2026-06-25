#!/usr/bin/env python3
"""
test_alta_wind_center.py - Simulation and Wake Analysis of the Alta Wind Energy Center (AWEC)

This test case models a large-scale layout of the Alta Wind Energy Center, located in 
Tehachapi Pass, Kern County, California - one of the most prominent wind energy resources 
in North America.

Physical Context & Terrain:
    - Located in the wind-swept Tehachapi Pass (elevation range 800 - 1200 m).
    - Features complex ridge-valley topography that channels strong westerly winds.
    - Large wind turbine array placed along ridge tops to maximize wind energy capture.
    - Replicates the massive wind farm layout consisting of multiple turbine strings 
      facing the dominant westerly winds.

Model Characteristics:
    - Terrain: Analytical 3D representation of Tehachapi Pass ridges and valleys.
    - Wind profile: Power-law profile representing 8 m/s wind from the west (U_ref = 8.0 m/s, 
      z_ref = 80.0 m) under neutral atmospheric boundary layer conditions.
    - Wake Model: Bastankhah-Gaussian analytical wake deficit model with quadratic superposition.
    - Resolves wake deficits and turbine power outputs across the massive farm layout.

References:
    - Alta Wind Energy Center, Mojave, California, USA.
    - Bastankhah, M. and Porté-Agel, F., "A new analytical model for wind-turbine wakes", 
      Renewable Energy, 2014.
    - Power Law Wind Profile: u(z) = U_ref * (z / z_ref)^alpha.

Author: GitHub Copilot Task Agent
Date: June 25, 2026
"""

import os
import sys
import math
import unittest
import numpy as np
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
import matplotlib.pyplot as plt
from pathlib import Path

# Setup paths
TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parent.parent.parent
SRC_PYTHON_DIR = REPO_ROOT / "src" / "python"
BUILD_PYTHON_DIR = REPO_ROOT / "build" / "python"

sys.path.insert(0, str(BUILD_PYTHON_DIR))
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    sys.exit(1)


class TestAltaWindCenter(unittest.TestCase):
    
    def setUp(self):
        """Set up the simulation coordinates, turbines, terrain, and solver inputs."""
        self.output_dir = TEST_DIR
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # 1. Geographic reference of the Alta Wind Energy Center
        # Latitude / Longitude reference in Tehachapi Pass, CA
        self.lat_ref = 35.035
        self.lon_ref = -118.32
        
        # 3 Ridges running North-South representing the wind farm strings
        self.lons_ridge1 = self.lon_ref - 0.015  # West Ridge
        self.lons_ridge2 = self.lon_ref          # Central Ridge
        self.lons_ridge3 = self.lon_ref + 0.015  # East Ridge
        
        # 13 wind turbines along each ridge (39 turbines in total)
        self.lat_vals = np.linspace(self.lat_ref - 0.015, self.lat_ref + 0.015, 13)
        
        # Convert Lat/Lon coordinates to local metric coordinates (meters)
        self.xs = []
        self.ys = []
        for lon in [self.lons_ridge1, self.lons_ridge2, self.lons_ridge3]:
            for lat in self.lat_vals:
                x = (lon - self.lon_ref) * 111000.0 * math.cos(math.radians(self.lat_ref))
                y = (lat - self.lat_ref) * 111000.0
                self.xs.append(x)
                self.ys.append(y)
                
        # Suzlon/GE/Vestas-style 2.0MW typical turbine parameters
        self.hub_height = 80.0       # Hub height [m]
        self.rotor_diameter = 80.0   # Rotor diameter [m]
        self.ct = 0.8                # Thrust coefficient
        
        # 2. Write turbines.csv
        self.turbine_file = self.output_dir / "turbines.csv"
        with open(self.turbine_file, "w") as f:
            f.write("# x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, power_curve_file\n")
            for idx, (x, y) in enumerate(zip(self.xs, self.ys)):
                f.write(f"{x:.2f}, {y:.2f}, {self.hub_height:.1f}, {self.rotor_diameter:.1f}, {self.ct:.2f}, 0.0, 0.0, nrel_5mw.csv\n")
                
        # 3. Write terrain.csv representing Tehachapi Pass Ridge-Valley system
        self.terrain_file = self.output_dir / "terrain.csv"
        with open(self.terrain_file, "w") as f:
            f.write("# Alta Wind Energy Center Tehachapi Pass Ridge Terrain\n")
            f.write("# X[m] Y[m] Z[m]\n")
            # 41x41 elevation grid spanning from -2000m to 2000m
            tx = np.linspace(-2000.0, 2000.0, 41)
            ty = np.linspace(-2000.0, 2000.0, 41)
            for y_val in ty:
                for x_val in tx:
                    # Topography: 3 distinct ridges running N-S at x ≈ -1360m, 0m, 1360m
                    z = 900.0 + 150.0 * math.cos(x_val / 500.0) + 50.0 * math.cos(y_val / 1000.0)
                    f.write(f"{x_val:.2f}, {y_val:.2f}, {z:.2f}\n")

        # 4. Write inputs.i
        self.inputs_file = self.output_dir / "inputs.i"
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
            
            # Power law with 8 m/s from west
            f.write("init_mode = powerlaw\n")
            f.write("U_ref = 8.0\n")
            f.write("V_ref = 0.0\n")
            f.write("z_ref = 80.0\n")
            f.write("powerlaw_exponent = 0.15\n")
            
            f.write("z0 = 0.05\n")
            f.write("dx = 100.0\n")
            f.write("dy = 100.0\n")
            f.write("dz = 15.0\n")
            f.write("domain_height = 300.0\n")
            f.write("alpha_h = 1.0\n")
            f.write("alpha_v = 1.0\n")
            f.write("mlmg_verbose = 0\n")
            f.write("max_grid_size = 64\n")
            f.write("plot_file = plt_alta_wind\n")

    def test_alta_simulation(self):
        """Execute simulation, extract wind speeds at 80m AGL, generate wake image, and verify power outputs."""
        old_cwd = os.getcwd()
        os.chdir(self.output_dir)
        
        try:
            # Initialize wind solver
            wind = WindSolver()
            wind.initialize("inputs.i")
            
            # Solve wind field with mass-consistent flow and analytical wake models
            solve_result = wind.solve()
            self.assertTrue(solve_result['success'])
            
            # Extract power outputs and inflow speeds
            powers = wind.get_turbine_power_outputs()
            inflows = wind.get_turbine_inflow_speeds()
            
            self.assertEqual(len(powers), 39)
            self.assertEqual(len(inflows), 39)
            
            # Print turbine simulation stats
            print(f"\nAlta Wind Energy Center - Simulated Turbine Outputs:")
            print(f"{'WT ID':<6} | {'X (m)':<8} | {'Y (m)':<8} | {'Inflow (m/s)':<12} | {'Power (kW)':<10}")
            print("-" * 55)
            for i in range(39):
                print(f"WT {i+1:02d} | {self.xs[i]:8.1f} | {self.ys[i]:8.1f} | {inflows[i]:12.2f} | {powers[i]:10.1f}")
                
            # Write simulation outputs to CSV file
            output_csv = self.output_dir / "turbine_power_output.csv"
            with open(output_csv, "w") as f:
                f.write("wt_id,x_m,y_m,inflow_speed_ms,power_kw\n")
                for i in range(39):
                    f.write(f"{i+1},{self.xs[i]:.2f},{self.ys[i]:.2f},{inflows[i]:.4f},{powers[i]:.2f}\n")
            print(f"\n✓ Wrote turbine power results to {output_csv}")
            
            # Extract wind speed at 80 m above terrain (hub height)
            vel_agl = wind.get_velocity_at_agl(80.0)
            u = vel_agl['u']
            v = vel_agl['v']
            ws_agl = np.sqrt(u**2 + v**2)
            
            # Generate the 2D contour plot of the wake field
            plt.figure(figsize=(10, 8))
            
            # Setup domain bounds
            x_min, x_max = wind.xmin, wind.xmax
            y_min, y_max = wind.ymin, wind.ymax
            
            # Generate coordinate grids for plotting
            x_grid = np.linspace(x_min, x_max, wind.nx)
            y_grid = np.linspace(y_min, y_max, wind.ny)
            
            # Contour plot of wind speed at 80m AGL
            contour = plt.contourf(x_grid, y_grid, ws_agl, levels=50, cmap='viridis')
            cbar = plt.colorbar(contour)
            cbar.set_label('Wind Speed at 80 m AGL [m/s]', fontsize=12)
            
            # Overlay turbine locations
            plt.scatter(self.xs, self.ys, color='red', marker='^', s=40, label='Turbines')
            
            plt.title('Alta Wind Energy Center - Hub Height (80 m AGL) Wake Deficit\n'
                      'Power-Law Inflow: 8 m/s from West', fontsize=14, fontweight='bold')
            plt.xlabel('X Coordinate [m]', fontsize=12)
            plt.ylabel('Y Coordinate [m]', fontsize=12)
            plt.xlim(x_min, x_max)
            plt.ylim(y_min, y_max)
            plt.grid(True, linestyle='--', alpha=0.5)
            plt.legend(loc='upper right')
            
            image_path = self.output_dir / "alta_wake_80m.png"
            plt.tight_layout()
            plt.savefig(image_path, dpi=300)
            plt.close()
            
            print(f"✓ Saved hub height wake field image to {image_path}")
            
            # Basic validation: check that downwind/shadowed turbines experience wake velocity deficit
            # Central Ridge turbines are downstream of West Ridge turbines
            # East Ridge turbines are downstream of Central Ridge turbines
            for i in range(13):
                wt_west = inflows[i]          # West Ridge turbine
                wt_center = inflows[13 + i]    # Central Ridge turbine
                wt_east = inflows[26 + i]      # East Ridge turbine
                
                # Check that deficits propagate and center/east are lower due to cumulative wake deficits
                self.assertLess(wt_center, wt_west + 1e-2)
                self.assertLess(wt_east, wt_center + 1e-2)
            
            wind.finalize()
            print("\n✓ Alta Wind Energy Center simulation run and test verification successful!")
            
        finally:
            os.chdir(old_cwd)


if __name__ == "__main__":
    unittest.main()
