#!/usr/bin/env python3
"""
plot_turbine_wake.py

Simulates the Happy Jack Wind Farm (Laramie County, WY) using the mass-consistent wind solver
and generates a high-resolution, publication-quality scenario gallery image showing the 
turbine wake deficits and interaction (Bastankhah deflection).

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
    print("Simulating Happy Jack Wind Farm (Wyoming)...")
    
    # Create a temporary directory for simulation files to avoid polluting repository
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_path = Path(tmp_dir)
        
        # 1. Define Happy Jack Wind Farm exact coordinates (lat, lon)
        lat_ref = 41.1413
        lon_ref = -105.0090
        
        # 14 turbines arranged in two rows
        lats = [41.1383, 41.1393, 41.1403, 41.1413, 41.1423, 41.1433, 41.1443,
                41.1383, 41.1393, 41.1403, 41.1413, 41.1423, 41.1433, 41.1443]
        lons = [-105.0110, -105.0110, -105.0110, -105.0110, -105.0110, -105.0110, -105.0110,
                -105.0070, -105.0070, -105.0070, -105.0070, -105.0070, -105.0070, -105.0070]
        
        # GE 1.5sle parameters
        D = 77.0  # Rotor diameter [m]
        H = 80.0  # Hub height [m]
        ct = 0.8  # Default Ct
        
        # 2. Convert coordinates to local meters relative to (lat_ref, lon_ref)
        xs = []
        ys = []
        for lat, lon in zip(lats, lons):
            # Flat-earth projection matching terrain_reader_srtm
            x = (lon - lon_ref) * 111000.0 * math.cos(math.radians(lat_ref))
            y = (lat - lat_ref) * 111000.0
            xs.append(x)
            ys.append(y)
            
        # 3. Write turbines.csv
        # Let's add 20 degrees of yaw to the western row to show deflection, 
        # and keep downstream (eastern) row aligned (0 degrees)
        turbine_file = tmp_path / "turbines.csv"
        with open(turbine_file, "w") as f:
            f.write("# x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, power_curve_file\n")
            for i, (x, y) in enumerate(zip(xs, ys)):
                # First 7 (western row) are yawed by 20.0 degrees
                yaw_val = 20.0 if i < 7 else 0.0
                f.write(f"{x:.2f}, {y:.2f}, {H:.1f}, {D:.1f}, {ct:.2f}, {yaw_val:.1f}, 0.0, nrel_5mw.csv\n")
                
        # Copy nrel_5mw.csv to tmp_dir
        orig_power_curve = REPO_ROOT / "tests_and_examples" / "randomized_hill_turbines" / "nrel_5mw.csv"
        if orig_power_curve.exists():
            shutil.copy(orig_power_curve, tmp_path / "nrel_5mw.csv")
        else:
            # Create a simple mock power curve if not found
            with open(tmp_path / "nrel_5mw.csv", "w") as f:
                f.write("# ws, power, ct\n")
                f.write("0.0, 0.0, 0.8\n")
                f.write("10.0, 1500.0, 0.8\n")
                f.write("25.0, 1500.0, 0.8\n")

        # 4. Write terrain.csv (flat high-altitude plateau in Wyoming)
        terrain_file = tmp_path / "terrain.csv"
        with open(terrain_file, "w") as f:
            f.write("# Happy Jack flat plateau terrain\n")
            f.write("-500.0, -800.0, 2200.0\n")
            f.write("1000.0, -800.0, 2200.0\n")
            f.write("-500.0,  800.0, 2200.0\n")
            f.write("1000.0,  800.0, 2200.0\n")

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
            f.write("dx = 20.0\n")
            f.write("dy = 20.0\n")
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
        
        # Get velocity field at hub height
        vel_hub = wind.get_velocity_at_agl(80.0)
        U = vel_hub['u']
        V = vel_hub['v']
        U_mag = np.sqrt(U**2 + V**2)

        # Generate coordinate meshes
        x_coords = xmin + (np.arange(nx) + 0.5) * dx
        y_coords = ymin + (np.arange(ny) + 0.5) * dy
        X, Y = np.meshgrid(x_coords, y_coords)

        # 7. Generate a beautiful visualization
        fig, ax = plt.subplots(figsize=(14, 8))
        
        # Plot velocity magnitude contours
        cp = ax.contourf(X, Y, U_mag, levels=40, cmap='viridis')
        cbar = fig.colorbar(cp, ax=ax, orientation='horizontal', pad=0.1, aspect=30)
        cbar.set_label('Wind Velocity at 80 m Hub Height [m/s]', fontsize=12, fontweight='bold')
        
        # Overlay wind streamlines to show yaw deflection
        ax.streamplot(X, Y, U, V, color=(1.0, 1.0, 1.0, 0.3), density=1.5, linewidth=1.0)

        # Plot wind turbines
        for i, (x_t, y_t) in enumerate(zip(xs, ys)):
            if i < 7:
                # Western row: Yawed
                angle = np.radians(20.0 + 90.0)
                dx_rotor = (D / 2.0) * np.cos(angle)
                dy_rotor = (D / 2.0) * np.sin(angle)
                ax.plot([x_t - dx_rotor, x_t + dx_rotor], [y_t - dy_rotor, y_t + dy_rotor], 
                        color='red', linewidth=3, label='Yawed Turbine (20°)' if i == 0 else "")
                ax.plot(x_t, y_t, 'ro', markersize=6)
                ax.text(x_t - 20, y_t + 15, f"WT{i+1}", color='white', fontsize=9, fontweight='bold')
            else:
                # Eastern row: Non-yawed
                ax.plot([x_t, x_t], [y_t - D/2.0, y_t + D/2.0], 
                        color='black', linewidth=3, label='Aligned Turbine' if i == 7 else "")
                ax.plot(x_t, y_t, 'ko', markersize=6)
                ax.text(x_t + 15, y_t + 15, f"WT{i+1}", color='white', fontsize=9, fontweight='bold')

        ax.set_title("Happy Jack Wind Farm Simulation (Wyoming) — Wake Deficit & Interaction\n"
                     "Upstream row yawed at 20° to deflect wakes away from downstream turbines", 
                     fontsize=15, fontweight='bold', pad=15)
        ax.set_xlabel("Local X distance [m]", fontsize=12)
        ax.set_ylabel("Local Y distance [m]", fontsize=12)
        ax.grid(True, linestyle='--', alpha=0.2)
        ax.legend(loc='lower left', framealpha=0.9, facecolor='darkgray', edgecolor='white')
        
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
