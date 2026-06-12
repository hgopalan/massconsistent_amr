#!/usr/bin/env python3
"""
plot_flatirons_windspeed.py

Generates a wind speed visualization for the Flatirons terrain scenario:
- Runs a modified flatirons_turbines regtest WITHOUT turbines
- Shows a vertical slice (X-Z) through the center of the domain
- Displays terrain-following wind speed contours and velocity vectors
- Demonstrates complex terrain wind interactions

Saves the generated image to docs/terrain_following_complex_flow.png.
"""

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import shutil

# Setup paths
POST_DIR = Path(__file__).resolve().parent
REPO_ROOT = POST_DIR.parent.parent
SRC_PYTHON_DIR = REPO_ROOT / "src" / "python"
TEST_DIR = REPO_ROOT / "regtest" / "terrain" / "flatirons_turbines"
DOCS_DIR = REPO_ROOT / "docs"

sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import wind_solver: {e}")
    print(f"Make sure Python bindings are built and PYTHONPATH includes {SRC_PYTHON_DIR}")
    sys.exit(1)

def main():
    print("Generating Flatirons terrain wind speed visualization (without turbines)...")
    
    # Change working directory to the test case folder
    os.chdir(TEST_DIR)
    
    # Create a temporary modified inputs file without turbines
    print("Creating modified inputs file without turbines...")
    with open("inputs.i", 'r') as f:
        original_content = f.read()
    
    # Create modified content - disable turbines
    modified_content = original_content
    modified_content = modified_content.replace("enable_turbine_wake = true", "enable_turbine_wake = false")
    
    # Save to temporary file
    temp_inputs = "inputs_no_turbines.i"
    with open(temp_inputs, 'w') as f:
        f.write(modified_content)
    
    # Initialize and solve
    print("Running wind solver without turbines...")
    wind = WindSolver(temp_inputs)
    wind.solve()
    
    # Get terrain
    terrain = wind.get_terrain()
    ny, nx = terrain.shape
    nz = wind.nz
    
    dx, dy, dz = wind.dx, wind.dy, wind.dz
    xmin, xmax = wind.xmin, wind.xmax
    ymin, ymax = wind.ymin, wind.ymax
    zmin, zmax = wind.zmin, wind.zmax
    
    x_coords = xmin + (np.arange(nx) + 0.5) * dx
    y_coords = ymin + (np.arange(ny) + 0.5) * dy
    z_coords = zmin + (np.arange(nz) + 0.5) * dz
    
    # Get 3D velocity field
    vel = wind.get_velocity()
    u = vel['u']  # shape (nz, ny, nx)
    v = vel['v']
    w = vel['w']
    
    # Calculate velocity magnitude
    mag = np.sqrt(u**2 + v**2 + w**2)
    
    # Select center Y slice for X-Z vertical plane
    j_mid = ny // 2
    
    # Grid for vertical slice plotting (X-Z)
    X_grid, Z_grid = np.meshgrid(x_coords, z_coords)
    
    # Slice arrays
    mag_slice = mag[:, j_mid, :]
    u_slice = u[:, j_mid, :]
    w_slice = w[:, j_mid, :]
    
    # The terrain profile at y_mid
    terrain_profile = terrain[j_mid, :]
    
    # Create Z coordinates matching actual grid
    Z_coords_3d = np.zeros((nz, nx))
    for k in range(nz):
        for i in range(nx):
            Z_coords_3d[k, i] = z_coords[k]
    
    # Mask out region below the terrain
    mask_below_terrain = Z_coords_3d < terrain_profile[np.newaxis, :]
    mag_masked = np.ma.masked_array(mag_slice, mask=mask_below_terrain)
    u_masked = np.ma.masked_array(u_slice, mask=mask_below_terrain)
    w_masked = np.ma.masked_array(w_slice, mask=mask_below_terrain)
    
    # Plotting
    plt.figure(figsize=(12, 6))
    
    # Plot wind speed contour
    levels = np.linspace(np.nanmin(mag_masked), np.nanmax(mag_masked), 20)
    cp = plt.contourf(X_grid, Z_grid, mag_masked, levels=levels, cmap='viridis', alpha=0.85)
    cbar = plt.colorbar(cp)
    cbar.set_label('Wind Speed Magnitude [m/s]', fontsize=11)
    
    # Plot terrain boundary as a thick line
    plt.plot(x_coords, terrain_profile, 'k-', linewidth=3, label='Terrain Surface')
    plt.fill_between(x_coords, zmin, terrain_profile, color='gray', alpha=0.5)
    
    # Plot velocity vectors on top
    # Sub-sample grid for clean quiver plot
    step_x = 2
    step_z = 2
    plt.quiver(X_grid[::step_z, ::step_x], Z_grid[::step_z, ::step_x],
               u_masked[::step_z, ::step_x], w_masked[::step_z, ::step_x],
               color='white', scale=150, width=0.003, alpha=0.9, label='Wind Vectors')
    
    plt.title("Flatirons Complex Terrain: Wind Speed Distribution (No Turbines)", 
             fontsize=14, fontweight='bold')
    plt.xlabel("X distance [m]", fontsize=11)
    plt.ylabel("Z height AGL/MSL [m]", fontsize=11)
    plt.xlim(xmin, xmax)
    plt.ylim(zmin, zmax)
    plt.legend(loc='upper right', fontsize=11)
    plt.grid(True, linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    
    # Save figure
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "terrain_following_complex_flow.png"
    plt.savefig(out_img, dpi=150)
    print(f"✓ Saved terrain-following wind speed plot to: {out_img}")
    plt.close()
    
    # Clean up temporary file
    if os.path.exists(temp_inputs):
        os.remove(temp_inputs)
    
    wind.finalize()

if __name__ == '__main__':
    main()
