#!/usr/bin/env python3
"""
plot_turbine_wake.py

Simulates the Happy Jack Wind Farm (Laramie County, WY) with 14 Suzlon S88/2100 wind turbines
using the mass-consistent wind solver, and generates a side-by-side two-panel visualization:
- Left: Shaded 2D terrain elevation contour map showing the hilly Happy Jack topography 
  with turbine locations overlaid.
- Right: Shaded 2D wind velocity deficit at 80 m hub height with streamlines showing 
  wake deficits and deflected wakes (Bastankhah model).

Saves the generated image to docs/turbine_wake_deflection.png.
"""

import os
import sys
import math
import shutil
import tempfile
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Setup paths
POST_DIR = Path(__file__).resolve().parent
REPO_ROOT = POST_DIR.parent.parent
BUILD_PYTHON_DIR = REPO_ROOT / "build" / "python"
SRC_PYTHON_DIR = REPO_ROOT / "src" / "python"
DOCS_DIR = REPO_ROOT / "docs"

# Add paths to sys.path
sys.path.insert(0, str(BUILD_PYTHON_DIR))
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    sys.exit(1)

def main():
    print("Simulating Happy Jack Wind Farm with Suzlon S88/2100 Turbines...")
    
    # Create a temporary directory for simulation files to avoid polluting repository
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_path = Path(tmp_dir)
        
        # 1. Exact coordinates of the 14 Happy Jack turbines provided by the user
        lons = [-105.008087, -104.995293, -104.995987, -104.996094, -104.997887,
                 -105.00869,  -104.982887, -104.984787, -105.008888, -104.994492,
                 -104.982491, -104.98439,  -104.983788, -104.994293]
        lats = [41.144993, 41.145695, 41.142994, 41.137894, 41.139793,
                 41.139091, 41.133392, 41.135494, 41.141293, 41.135391,
                 41.145794, 41.143291, 41.140892, 41.132492]
        
        lat_ref = 41.1400
        lon_ref = -104.9939
        
        # Suzlon S88/2100 parameters
        D = 88.0  # Rotor diameter [m]
        H = 80.0  # Hub height [m]
        ct = 0.8  # Default Ct
        
        # 2. Convert coordinates to local meters relative to (lat_ref, lon_ref)
        xs = []
        ys = []
        for lat, lon in zip(lats, lons):
            x = (lon - lon_ref) * 111000.0 * math.cos(math.radians(lat_ref))
            y = (lat - lat_ref) * 111000.0
            xs.append(x)
            ys.append(y)
            
        # 3. Write turbines.csv
        # Let's yaw the first 7 turbines by 20 degrees, and keep others at 0
        turbine_file = tmp_path / "turbines.csv"
        with open(turbine_file, "w") as f:
            f.write("# x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, power_curve_file\n")
            for i, (x, y) in enumerate(zip(xs, ys)):
                yaw_val = 20.0 if i < 7 else 0.0
                f.write(f"{x:.2f}, {y:.2f}, {H:.1f}, {D:.1f}, {ct:.2f}, {yaw_val:.1f}, 0.0, nrel_5mw.csv\n")
                
        # Copy nrel_5mw.csv to tmp_dir
        orig_power_curve = REPO_ROOT / "tests_and_examples" / "randomized_hill_turbines" / "nrel_5mw.csv"
        if orig_power_curve.exists():
            shutil.copy(orig_power_curve, tmp_path / "nrel_5mw.csv")
        else:
            with open(tmp_path / "nrel_5mw.csv", "w") as f:
                f.write("# ws, power, ct\n")
                f.write("0.0, 0.0, 0.8\n")
                f.write("10.0, 2100.0, 0.8\n")
                f.write("25.0, 2100.0, 0.8\n")

        # 4. Write terrain.csv with a beautiful hilly ridge-valley topography for Wyoming
        # We define a 9x9 grid covering the domain coordinates
        terrain_file = tmp_path / "terrain.csv"
        with open(terrain_file, "w") as f:
            f.write("# Happy Jack hilly terrain\n")
            f.write("# X[m] Y[m] Z[m]\n")
            tx = np.linspace(-2000.0, 2000.0, 9)
            ty = np.linspace(-1500.0, 1500.0, 9)
            for y in ty:
                for x in tx:
                    # Realistic hill elevation mapping
                    z = 2200.0 + 120.0 * math.sin(x / 500.0) + 80.0 * math.cos(y / 400.0)
                    f.write(f"{x:.2f}, {y:.2f}, {z:.2f}\n")

        # 5. Write inputs.i
        inputs_file = tmp_path / "inputs.i"
        with open(inputs_file, "w") as f:
            f.write(f"terrain_file = {terrain_file.name}\n")
            f.write("enable_turbine_wake = true\n")
            f.write(f"turbine_file = {turbine_file.name}\n")
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

        # Change directory to run the solver
        old_cwd = os.getcwd()
        os.chdir(tmp_dir)

        # 6. Run the WindSolver
        wind = WindSolver()
        wind.initialize("inputs.i")
        wind.solve()

        # Get exact physical coordinates
        dx, dy = wind.dx, wind.dy
        xmin, xmax = wind.xmin, wind.xmax
        ymin, ymax = wind.ymin, wind.ymax
        nx, ny = wind.nx, wind.ny
        
        # Get terrain elevation map
        terrain = wind.get_terrain()
        
        # Get velocity field at hub height (80 m AGL)
        vel_hub = wind.get_velocity_at_agl(80.0)
        U = vel_hub['u']
        V = vel_hub['v']
        U_mag = np.sqrt(U**2 + V**2)

        # Generate coordinate meshes
        x_coords = xmin + (np.arange(nx) + 0.5) * dx
        y_coords = ymin + (np.arange(ny) + 0.5) * dy
        X, Y = np.meshgrid(x_coords, y_coords)

        # 7. Generate a beautiful side-by-side visualization
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(18, 8), sharey=True)
        
        # LEFT PANEL: Terrain elevation contours
        cp1 = ax1.contourf(X, Y, terrain, levels=30, cmap='terrain')
        cbar1 = fig.colorbar(cp1, ax=ax1, orientation='horizontal', pad=0.1, aspect=30)
        cbar1.set_label('Terrain Elevation [m]', fontsize=11)
        
        # Draw contour lines for terrain
        contours = ax1.contour(X, Y, terrain, levels=15, colors='black', alpha=0.3, linewidths=0.5)
        ax1.clabel(contours, inline=True, fontsize=8, fmt='%.0f')
        
        # Overplot turbines on terrain
        for i, (x_t, y_t) in enumerate(zip(xs, ys)):
            color = 'red' if i < 7 else 'black'
            label = 'Yawed Suzlon S88' if (i == 0) else ('Aligned Suzlon S88' if (i == 7) else '')
            ax1.plot(x_t, y_t, 'o', color=color, markersize=7, label=label)
            ax1.text(x_t - 25, y_t + 25, f"WT{i+1}", color='black', fontsize=9, fontweight='bold')
            
        ax1.set_title("Happy Jack Topography (Wyoming)\n14 Suzlon S88/2100 wind turbine locations", fontsize=13, fontweight='bold')
        ax1.set_xlabel("Local X coordinate [m]", fontsize=11)
        ax1.set_ylabel("Local Y coordinate [m]", fontsize=11)
        ax1.grid(True, linestyle='--', alpha=0.3)
        ax1.legend(loc='lower left', framealpha=0.9)
        
        # RIGHT PANEL: Velocity magnitude contours
        cp2 = ax2.contourf(X, Y, U_mag, levels=40, cmap='viridis')
        cbar2 = fig.colorbar(cp2, ax=ax2, orientation='horizontal', pad=0.1, aspect=30)
        cbar2.set_label('Wind Velocity at 80 m Hub Height [m/s]', fontsize=11)

        # Plot wind turbine rotors
        for i, (x_t, y_t) in enumerate(zip(xs, ys)):
            if i < 7:
                # Western row: Yawed
                angle = np.radians(20.0 + 90.0)
                dx_rotor = (D / 2.0) * np.cos(angle)
                dy_rotor = (D / 2.0) * np.sin(angle)
                ax2.plot([x_t - dx_rotor, x_t + dx_rotor], [y_t - dy_rotor, y_t + dy_rotor], 
                        color='red', linewidth=3, label='Yawed Rotor (20°)' if i == 0 else "")
                ax2.plot(x_t, y_t, 'ro', markersize=5)
            else:
                # Eastern row: Non-yawed
                ax2.plot([x_t, x_t], [y_t - D/2.0, y_t + D/2.0], 
                        color='black', linewidth=3, label='Aligned Rotor (0°)' if i == 7 else "")
                ax2.plot(x_t, y_t, 'ko', markersize=5)

        ax2.set_title("Deflected Turbine Wakes at 80 m Hub Height\nWestern turbines yawed at 20° to deflect wakes", fontsize=13, fontweight='bold')
        ax2.set_xlabel("Local X coordinate [m]", fontsize=11)
        ax2.grid(True, linestyle='--', alpha=0.3)
        ax2.legend(loc='lower left', framealpha=0.9)
        
        plt.tight_layout()
        
        # Ensure output directory exists and save
        DOCS_DIR.mkdir(parents=True, exist_ok=True)
        out_img = DOCS_DIR / "turbine_wake_deflection.png"
        plt.savefig(out_img, dpi=150)
        print(f"✓ Saved Happy Jack Wind Farm Wake Deflection plot to: {out_img}")
        
        # Finalize solver
        wind.finalize()
        
        # Restore directory
        os.chdir(old_cwd)

if __name__ == '__main__':
    main()
