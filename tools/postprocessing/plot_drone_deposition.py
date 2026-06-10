#!/usr/bin/env python3
"""
plot_drone_deposition.py

Runs the Colorado Complex Terrain Drone Spray workflow and generates a nice two-panel figure:
- Left: Nice 2D terrain elevation map with flight path overlaid.
- Right: 2D total pesticide deposition density contour map with flight path.

Saves the generated image to docs/drone_deposition_plot.png.
"""

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Setup paths
POST_DIR = Path(__file__).resolve().parent
REPO_ROOT = POST_DIR.parent.parent
SRC_PYTHON_DIR = REPO_ROOT / "src" / "python"
TEST_DIR = REPO_ROOT / "tests_and_examples" / "colorado_drone_spray"
DOCS_DIR = REPO_ROOT / "docs"

sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
    from agricultural_drone import (
        DroneTrajectory, MassEmissionRegulator, DronePuffDispersion
    )
except ImportError as e:
    print(f"ERROR: Could not import wind_solver/agricultural_drone: {e}")
    print("Ensure PYTHONPATH includes build/python and src/python.")
    sys.exit(1)

def main():
    print("Running Colorado Complex Terrain Drone Spray simulation...")
    # Change working directory to the test case folder to load inputs.i, terrain.csv, etc.
    os.chdir(TEST_DIR)
    
    # Initialize and solve mass-consistent wind solver
    wind = WindSolver("inputs.i")
    wind.solve()
    
    # Get terrain elevation and grid coordinates
    terrain = wind.get_terrain()
    ny, nx = terrain.shape
    
    # Get physical coordinates
    dx, dy = wind.dx, wind.dy
    xmin, xmax = wind.xmin, wind.xmax
    ymin, ymax = wind.ymin, wind.ymax
    
    x_coords = xmin + (np.arange(nx) + 0.5) * dx
    y_coords = ymin + (np.arange(ny) + 0.5) * dy
    
    # Define trajectory
    x_pts = np.array([-150.0, -75.0, 0.0, 75.0, 150.0])
    y_pts = np.array([0.0, 0.0, 0.0, 0.0, 0.0])
    
    def get_terrain_elevation(x, y):
        i_idx = int((x - wind.xmin) / wind.dx)
        j_idx = int((y - wind.ymin) / wind.dy)
        i_idx = max(0, min(nx - 1, i_idx))
        j_idx = max(0, min(ny - 1, j_idx))
        return float(terrain[j_idx, i_idx])
        
    z_pts = np.array([get_terrain_elevation(x, y) + 3.0 for x, y in zip(x_pts, y_pts)])
    times = np.array([0.0, 10.0, 20.0, 30.0, 40.0])
    speeds = np.full_like(times, 7.5)
    headings = np.full_like(times, 0.0)
    flow_rates = np.full_like(times, 2.0)
    active_flags = np.full_like(times, True, dtype=bool)
    
    trajectory = DroneTrajectory(
        times=times, x_pts=x_pts, y_pts=y_pts, z_pts=z_pts,
        speeds=speeds, headings=headings, flow_rates=flow_rates, active_flags=active_flags
    )
    
    regulator = MassEmissionRegulator(
        formulation_density=1000.0,
        active_fraction=0.1,
        base_speed=7.5,
        speed_dependent=False,
        droplet_bins={
            'fine': {'diameter': 60e-6, 'fraction': 0.15},
            'medium': {'diameter': 150e-6, 'fraction': 0.50},
            'coarse': {'diameter': 300e-6, 'fraction': 0.35}
        }
    )
    
    # Run puff dispersion simulation
    puff_model = DronePuffDispersion(
        xmin=wind.xmin, xmax=wind.xmax,
        ymin=wind.ymin, ymax=wind.ymax,
        zmin=wind.zmin, zmax=wind.zmax,
        dx=wind.dx, dy=wind.dy, dz=wind.dz
    )
    
    puff_model.simulate(
        trajectory=trajectory,
        regulator=regulator,
        wind_solver=wind,
        dt=0.5,
        K_h=1.0,
        K_v=0.5,
        sigma_y0=0.5,
        sigma_z0=0.5,
        enable_ground_reflection=True,
        enable_settling=True,
        enable_evaporation=True,
        enable_degradation=False,
        enable_canopy_interception=True,
        canopy_height=1.2,
        leaf_area_index=2.5,
        frontal_area_index=0.6
    )
    
    # Total deposition (ground + canopy compartments)
    total_deposition = (
        puff_model.ground_deposition +
        puff_model.canopy_top_deposition +
        puff_model.lower_foliage_deposition
    )
    
    # Plotting
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6), sharey=True)
    
    # Grid for contour plotting
    X, Y = np.meshgrid(x_coords, y_coords)
    
    # Left subplot: Terrain Map
    cp1 = ax1.contourf(X, Y, terrain, levels=20, cmap='terrain')
    cbar1 = fig.colorbar(cp1, ax=ax1, orientation='vertical')
    cbar1.set_label('Elevation [m]', fontsize=11)
    
    # Add contour lines for terrain
    contours1 = ax1.contour(X, Y, terrain, levels=10, colors='black', alpha=0.3, linewidths=0.7)
    ax1.clabel(contours1, inline=True, fontsize=8, fmt='%.1f')
    
    # Plot drone flight path
    ax1.plot(x_pts, y_pts, 'ro--', label='Flight Path', linewidth=2, markersize=8)
    # Mark target swath limits (Y = -20m to +20m)
    ax1.axhline(-20.0, color='blue', linestyle=':', label='Target Swath Bounds')
    ax1.axhline(20.0, color='blue', linestyle=':')
    
    ax1.set_title("Colorado Complex Terrain Map", fontsize=14, fontweight='bold')
    ax1.set_xlabel("X coordinate [m]", fontsize=11)
    ax1.set_ylabel("Y coordinate [m]", fontsize=11)
    ax1.grid(True, linestyle='--', alpha=0.5)
    ax1.legend(loc='lower left', framealpha=0.9)
    
    # Right subplot: Pesticide Deposition
    cp2 = ax2.contourf(X, Y, total_deposition, levels=20, cmap='YlGnBu')
    cbar2 = fig.colorbar(cp2, ax=ax2, orientation='vertical')
    cbar2.set_label('Total Deposition [g]', fontsize=11)
    
    # Add flight path on right plot as well
    ax2.plot(x_pts, y_pts, 'ro--', label='Flight Path', linewidth=2, markersize=8)
    ax2.axhline(-20.0, color='blue', linestyle=':', label='Target Swath Bounds')
    ax2.axhline(20.0, color='blue', linestyle=':')
    
    ax2.set_title("Agricultural Pesticide Deposition Map", fontsize=14, fontweight='bold')
    ax2.set_xlabel("X coordinate [m]", fontsize=11)
    ax2.grid(True, linestyle='--', alpha=0.5)
    ax2.legend(loc='lower left', framealpha=0.9)
    
    plt.tight_layout()
    
    # Make sure output directory exists
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "drone_deposition_plot.png"
    plt.savefig(out_img, dpi=150)
    print(f"Saved nice terrain & deposition plot to: {out_img}")
    
    # Cleanup wind solver
    wind.finalize()

if __name__ == '__main__':
    main()
