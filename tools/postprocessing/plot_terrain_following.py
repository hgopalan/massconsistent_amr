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
    
    # Select horizontal slice at a height above ground (e.g., 50 m AGL)
    # Find the closest k index to 50 m
    target_height = 50.0
    k_closest = np.argmin(np.abs(z_coords - target_height))
    actual_height = z_coords[k_closest]
    
    # Grid for horizontal slice plotting (X-Y)
    X_grid, Y_grid = np.meshgrid(x_coords, y_coords)
    
    # Slice arrays at this height
    mag_slice = mag[k_closest, :, :]
    
    # Plotting
    plt.figure(figsize=(10, 8))
    
    # Mask out region below terrain at this height
    # Create terrain mask at this height
    terrain_at_height = terrain.copy()
    mask_below_terrain = terrain_at_height > actual_height
    mag_masked = np.ma.masked_array(mag_slice, mask=mask_below_terrain)
    
    # Plot wind speed contour (horizontal slice)
    cp = plt.contourf(X_grid, Y_grid, mag_masked, levels=20, cmap='viridis', alpha=0.85)
    cbar = plt.colorbar(cp)
    cbar.set_label('Wind Speed Magnitude [m/s]', fontsize=11)
    
    # Overlay terrain elevation contours
    contour_lines = plt.contour(X_grid, Y_grid, terrain_at_height, levels=10, colors='black', 
                                 linewidths=0.5, alpha=0.4, linestyles='--')
    plt.clabel(contour_lines, inline=True, fontsize=8, fmt='%.0f m')
    
    plt.title(f"Terrain-Following Wind Flow Over Gaussian Hill (at {actual_height:.0f} m AGL)", 
              fontsize=14, fontweight='bold')
    plt.xlabel("X distance [m]", fontsize=11)
    plt.ylabel("Y distance [m]", fontsize=11)
    plt.xlim(xmin, xmax)
    plt.ylim(ymin, ymax)
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
