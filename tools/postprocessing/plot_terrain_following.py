#!/usr/bin/env python3
"""
plot_terrain_following.py

Runs the Gaussian Hill case and generates a nice visualization of terrain-following flow:
- Vertical slice (X-Z) through the center of the domain.
- Show contour of wind velocity magnitude.
- Overlay streamline flow lines or velocity vectors showing compression over the hill.

Saves the generated image to docs/terrain_following_complex_flow.png.
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
TEST_DIR = REPO_ROOT / "regtest" / "terrain" / "gaussian_hill"
DOCS_DIR = REPO_ROOT / "docs"

sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import wind_solver: {e}")
    sys.exit(1)

def main():
    print("Running Gaussian Hill terrain-following wind simulation...")
    # Change working directory to the test case folder
    os.chdir(TEST_DIR)
    
    # Initialize and solve
    wind = WindSolver("inputs.i")
    wind.solve()
    
    # Get terrain and coordinates
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
    
    # Select center Y slice
    j_mid = ny // 2
    
    # Grid for vertical slice plotting (X-Z)
    X_grid, Z_grid = np.meshgrid(x_coords, z_coords)
    
    # Slice arrays
    mag_slice = mag[:, j_mid, :]
    u_slice = u[:, j_mid, :]
    w_slice = w[:, j_mid, :]
    
    # Plotting
    plt.figure(figsize=(10, 5))
    
    # Mask out region below the terrain (if any)
    # The terrain profile at y_mid
    terrain_profile = terrain[j_mid, :]
    
    # Create Z coordinates matching terrain-following coordinate transformation
    # If the solver is strictly terrain-following, the grid cells are at Z_ij = z_s(i,j) + k * dz_local?
    # In AMReX-based massconsistent_amr, the cells are standard Cartesians, but cells below terrain are masked/solid.
    # Let's plot actual cartesian grid but mask the cells where z_center < terrain_elevation
    Z_coords_3d = np.zeros((nz, nx))
    for k in range(nz):
        for i in range(nx):
            Z_coords_3d[k, i] = z_coords[k]
            
    mask_below_terrain = Z_coords_3d < terrain_profile[np.newaxis, :]
    mag_masked = np.ma.masked_array(mag_slice, mask=mask_below_terrain)
    u_masked = np.ma.masked_array(u_slice, mask=mask_below_terrain)
    w_masked = np.ma.masked_array(w_slice, mask=mask_below_terrain)
    
    # Plot wind speed contour
    cp = plt.contourf(X_grid, Z_grid, mag_masked, levels=20, cmap='viridis', alpha=0.85)
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
    
    plt.title("Terrain-Following Wind Flow Over Gaussian Hill", fontsize=14, fontweight='bold')
    plt.xlabel("X distance [m]", fontsize=11)
    plt.ylabel("Z height AGL/MSL [m]", fontsize=11)
    plt.xlim(xmin, xmax)
    plt.ylim(zmin, zmax)
    plt.legend(loc='upper right')
    plt.grid(True, linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "terrain_following_complex_flow.png"
    plt.savefig(out_img, dpi=150)
    print(f"Saved terrain-following flow plot to: {out_img}")
    
    wind.finalize()

if __name__ == '__main__':
    main()
