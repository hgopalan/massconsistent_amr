#!/usr/bin/env python3
"""
test_alta_wind_center.py - Simulation and Wake Analysis of the Alta Wind Energy Center (AWEC)

This test case models the full layout of the Alta Wind Energy Center (AWEC), located in 
Tehachapi Pass, Kern County, California. It features 600 wind turbines in UTM Zone 11N coordinates
distributed along multiple mountain ridges.

Physical Context & Terrain:
    - Located in the wind-swept Tehachapi Pass (elevation range 800 - 1200 m).
    - Features complex ridge-valley topography that channels strong westerly winds.
    - Large wind turbine array placed along ridge tops to maximize wind energy capture.
    - Uses UTM Zone 11N projection coordinates for realistic geographic mapping.

Model Characteristics:
    - Terrain: Analytical 3D representation of Tehachapi Pass ridges and valleys in UTM coordinates.
    - Wind profile: Power-law profile representing 8 m/s wind from the west (U_ref = 8.0 m/s, 
      z_ref = 80.0 m) under neutral atmospheric boundary layer conditions.
    - Wake Model: Bastankhah-Gaussian analytical wake deficit model with quadratic superposition.
    - Resolves wake deficits and turbine power outputs across 600 turbines.
    - Uses grid spacing with aspect ratio of exactly 8.0 (dx = dy = 120m, dz = 15m).
    - Employs AMReX Nodal Projection solver tuning (16 pre- and post-smoothing steps) 
      to ensure perfect convergence at high aspect ratios.

References:
    - Alta Wind Energy Center, Mojave, California, USA.
    - Bastankhah, M. and Porté-Agel, F., "A new analytical model for wind-turbine wakes", 
      Renewable Energy, 2014.
    - Power Law Wind Profile: u(z) = U_ref * (z / z_ref)^alpha.
"""

import os
import sys
import unittest
import math
import numpy as np
import pyproj
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
        
        # 1. Geographic reference of the Alta Wind Energy Center (Tehachapi Pass, CA)
        self.lat_ref = 35.035
        self.lon_ref = -118.32
        
        # Define projection to UTM Zone 11N (California)
        self.proj = pyproj.Proj(proj='utm', zone=11, ellps='WGS84', hemisphere='north')
        
        # 6 Rows of turbines running North-South representing the wind farm strings
        self.lons_list = np.linspace(self.lon_ref - 0.010, self.lon_ref + 0.010, 6)
        
        # 100 wind turbines along each row (600 turbines in total)
        self.lats_list = np.linspace(self.lat_ref - 0.010, self.lat_ref + 0.010, 100)
        
        # Convert Lat/Lon coordinates to UTM Easting/Northing coordinates
        self.xs = []
        self.ys = []
        for lon in self.lons_list:
            for lat in self.lats_list:
                easting, northing = self.proj(lon, lat)
                self.xs.append(easting)
                self.ys.append(northing)
                
        # Suzlon/GE/Vestas-style 2.0MW typical turbine parameters
        self.hub_height = 80.0       # Hub height [m]
        self.rotor_diameter = 80.0   # Rotor diameter [m]
        self.ct = 0.8                # Thrust coefficient
        
        # Find bounds of turbines in UTM
        self.e_min, self.e_max = min(self.xs), max(self.xs)
        self.n_min, self.n_max = min(self.ys), max(self.ys)
        
        # Add a 600m buffer around the turbine bounding box for the terrain domain
        self.xmin = self.e_min - 600.0
        self.xmax = self.e_max + 600.0
        self.ymin = self.n_min - 600.0
        self.ymax = self.n_max + 600.0
        
        self.x_center = (self.xmin + self.xmax) / 2.0
        self.y_center = (self.ymin + self.ymax) / 2.0
        
        # 2. Write turbines.csv
        self.turbine_file = self.output_dir / "turbines.csv"
        with open(self.turbine_file, "w") as f:
            f.write("# x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, power_curve_file\n")
            for x, y in zip(self.xs, self.ys):
                f.write(f"{x:.2f}, {y:.2f}, {self.hub_height:.1f}, {self.rotor_diameter:.1f}, {self.ct:.2f}, 0.0, 0.0, nrel_5mw.csv\n")
                
        # 3. Write terrain.csv representing Tehachapi Pass Ridge-Valley system in UTM coordinates
        self.terrain_file = self.output_dir / "terrain.csv"
        with open(self.terrain_file, "w") as f:
            f.write("# Alta Wind Energy Center Tehachapi Pass Ridge Terrain in UTM Coordinates\n")
            f.write("# X[m] Y[m] Z[m]\n")
            # 41x41 elevation grid spanning the UTM domain
            tx = np.linspace(self.xmin, self.xmax, 41)
            ty = np.linspace(self.ymin, self.ymax, 41)
            for y_val in ty:
                for x_val in tx:
                    # Calculate terrain relative to domain center to generate ridges/valleys
                    x_rel = x_val - self.x_center
                    y_rel = y_val - self.y_center
                    # Topography: 6 distinct ridges running N-S
                    z = 900.0 + 150.0 * math.cos(x_rel / 500.0) + 50.0 * math.cos(y_rel / 1000.0)
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
            # Aspect ratio of exactly 8.0 (dx = dy = 120m, dz = 15m)
            f.write("dx = 120.0\n")
            f.write("dy = 120.0\n")
            f.write("dz = 15.0\n")
            f.write("domain_height = 300.0\n")
            f.write("alpha_h = 1.0\n")
            f.write("alpha_v = 1.0\n")
            f.write("mlmg_verbose = 0\n")
            f.write("max_grid_size = 64\n")
            f.write("plot_file = plt_alta_wind\n")
            
            # Nodal projection smoothing tuning for high aspect ratio of 8.0
            f.write("nodal_proj.num_pre_smooth = 16\n")
            f.write("nodal_proj.num_post_smooth = 16\n")

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
            
            self.assertEqual(len(powers), 600)
            self.assertEqual(len(inflows), 600)
            
            # Print a subset of turbine simulation stats for diagnostics
            print(f"\nAlta Wind Energy Center (600 Turbines, UTM Coordinates) - Simulated Outputs (Subset):")
            print(f"{'WT ID':<6} | {'Easting (m)':<12} | {'Northing (m)':<12} | {'Inflow (m/s)':<12} | {'Power (kW)':<10}")
            print("-" * 65)
            # Show first 5 and last 5 turbines
            indices_to_show = list(range(5)) + list(range(595, 600))
            for i in indices_to_show:
                print(f"WT {i+1:03d} | {self.xs[i]:12.1f} | {self.ys[i]:12.1f} | {inflows[i]:12.2f} | {powers[i]:10.1f}")
                
            # Write simulation outputs to CSV file
            output_csv = self.output_dir / "turbine_power_output.csv"
            with open(output_csv, "w") as f:
                f.write("wt_id,easting_m,northing_m,inflow_speed_ms,power_kw\n")
                for i in range(600):
                    f.write(f"{i+1},{self.xs[i]:.2f},{self.ys[i]:.2f},{inflows[i]:.4f},{powers[i]:.2f}\n")
            print(f"\n✓ Wrote turbine power results to {output_csv}")
            
            # Extract wind speed at 80 m above terrain (hub height)
            vel_agl = wind.get_velocity_at_agl(80.0)
            u = vel_agl['u']
            v = vel_agl['v']
            ws_agl = np.sqrt(u**2 + v**2)
            
            # Generate the 2D contour plot of the wake field in UTM coordinates
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
            plt.scatter(self.xs, self.ys, color='red', marker='^', s=10, alpha=0.6, label='600 Turbines')
            
            plt.title('Alta Wind Energy Center - Hub Height (80 m AGL) Wake Deficit\n'
                      '600 Turbines in UTM coordinates; Wind from West (8 m/s)', fontsize=13, fontweight='bold')
            plt.xlabel('Easting [m] (UTM Zone 11N)', fontsize=12)
            plt.ylabel('Northing [m] (UTM Zone 11N)', fontsize=12)
            plt.xlim(x_min, x_max)
            plt.ylim(y_min, y_max)
            plt.grid(True, linestyle='--', alpha=0.5)
            plt.legend(loc='upper right')
            
            image_path = self.output_dir / "alta_wake_80m.png"
            plt.tight_layout()
            plt.savefig(image_path, dpi=300)
            plt.close()
            
            print(f"✓ Saved hub height wake field image to {image_path}")
            
            # Verify that cumulative deficits propagation exists
            # Upstream row should have near-free-stream speeds, downstream rows should have significant deficits
            avg_west = np.mean(inflows[0:100])
            avg_center = np.mean(inflows[200:300])
            avg_east = np.mean(inflows[500:600])
            
            # West row is upstream and should be close to 8.0 m/s
            self.assertGreater(avg_west, 6.5)
            # Center and East rows are downstream and should experience significant deficits (well below 6.0 m/s)
            self.assertLess(avg_center, 6.0)
            self.assertLess(avg_east, 6.0)
            
            wind.finalize()
            print("\n✓ Alta Wind Energy Center simulation run and test verification successful!")
            
        finally:
            os.chdir(old_cwd)


if __name__ == "__main__":
    unittest.main()
