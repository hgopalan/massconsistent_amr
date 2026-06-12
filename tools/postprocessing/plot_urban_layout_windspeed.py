#!/usr/bin/env python3
"""
plot_urban_layout_windspeed.py

Generates a wind speed visualization for the Urban Layout scenario:
- Runs the urban_layout regtest to compute wind field
- Shows a horizontal (plan) view of wind speed magnitude at a selected height
- Overlays building geometries
- Displays wind velocity vectors

Saves the generated image to docs/urban_street_canyon.png.
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
TEST_DIR = REPO_ROOT / "regtest" / "buildings" / "urban_layout"
BUILD_DIR = REPO_ROOT / "build"
DOCS_DIR = REPO_ROOT / "docs"

sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import wind_solver: {e}")
    print(f"Make sure Python bindings are built and PYTHONPATH includes {SRC_PYTHON_DIR}")
    sys.exit(1)

def parse_buildings_csv(filename):
    """Parse buildings.csv and return list of building geometries with heights."""
    buildings = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            # Parse building geometry
            if 'POLYGON:' in line:
                # Complex polygon format: POLYGON: x1 y1 x2 y2 ... xn yn | zmin zmax
                parts = line.split('|')
                coords_part = parts[0].replace('POLYGON:', '').strip()
                height_part = parts[1].strip()
                
                # Parse coordinates
                coords_list = [float(x) for x in coords_part.split()]
                coords = [(coords_list[i], coords_list[i+1]) for i in range(0, len(coords_list), 2)]
                
                # Parse heights
                zmin, zmax = map(float, height_part.split())
                buildings.append({
                    'type': 'polygon',
                    'coords': coords,
                    'zmin': zmin,
                    'zmax': zmax,
                    'height': zmax - zmin
                })
            else:
                # Regular box format: xmin xmax ymin ymax zmin zmax
                parts = [float(x) for x in line.split()]
                if len(parts) == 6:
                    xmin, xmax, ymin, ymax, zmin, zmax = parts
                    buildings.append({
                        'type': 'box',
                        'xmin': xmin,
                        'xmax': xmax,
                        'ymin': ymin,
                        'ymax': ymax,
                        'zmin': zmin,
                        'zmax': zmax,
                        'height': zmax - zmin
                    })
    return buildings

def main():
    print("Generating Urban Layout wind speed visualization...")
    
    # Change working directory to the test case folder
    os.chdir(TEST_DIR)
    
    # Initialize and solve
    print("Running wind solver...")
    wind = WindSolver("inputs.i")
    wind.solve()
    
    # Get domain parameters
    ny, nx = wind.ny, wind.nx
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
    
    # Select a height slightly above the buildings (around 20m AGL)
    # Buildings are up to ~40m, so select a height above that
    target_height = 50.0  # 50m AGL
    k_slice = int((target_height - zmin) / dz)
    k_slice = min(max(k_slice, 0), nz - 1)
    actual_height = z_coords[k_slice]
    
    # Horizontal slice at this height
    mag_slice = mag[k_slice, :, :]  # shape (ny, nx)
    u_slice = u[k_slice, :, :]
    v_slice = v[k_slice, :, :]
    
    # Create meshgrid for plotting
    X, Y = np.meshgrid(x_coords, y_coords)
    
    # Parse buildings
    buildings = parse_buildings_csv("buildings.csv")
    
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 10))
    
    # Plot wind speed magnitude as contours
    levels = np.linspace(mag_slice.min(), mag_slice.max(), 20)
    cf = ax.contourf(X, Y, mag_slice, levels=levels, cmap='viridis', alpha=0.8)
    cbar = fig.colorbar(cf, ax=ax)
    cbar.set_label(f'Wind Speed Magnitude [m/s] at {actual_height:.1f} m AGL', fontsize=11)
    
    # Overlay wind velocity vectors
    step_x = 2
    step_y = 2
    ax.quiver(X[::step_y, ::step_x], Y[::step_y, ::step_x],
             u_slice[::step_y, ::step_x], v_slice[::step_y, ::step_x],
             color='white', scale=150, width=0.003, alpha=0.8)
    
    # Draw building footprints
    from matplotlib.patches import Rectangle, Polygon as MPLPolygon
    from matplotlib.patches import Patch
    
    box_patches = []
    poly_patches = []
    
    for building in buildings:
        if building['type'] == 'box':
            # Draw box building footprint
            rect = Rectangle(
                (building['xmin'], building['ymin']),
                building['xmax'] - building['xmin'],
                building['ymax'] - building['ymin'],
                linewidth=2,
                edgecolor='red',
                facecolor='none',
                alpha=0.8
            )
            ax.add_patch(rect)
            box_patches.append(rect)
        elif building['type'] == 'polygon':
            # Draw polygon building footprint
            coords = building['coords']
            polygon = MPLPolygon(coords, linewidth=2, edgecolor='red',
                               facecolor='none', alpha=0.8)
            ax.add_patch(polygon)
            poly_patches.append(polygon)
    
    # Set axis properties
    ax.set_xlim(xmin, xmax)
    ax.set_ylim(ymin, ymax)
    ax.set_aspect('equal')
    ax.grid(True, linestyle='--', alpha=0.3)
    ax.set_xlabel("X distance [m]", fontsize=12)
    ax.set_ylabel("Y distance [m]", fontsize=12)
    ax.set_title(f"Urban Layout: Wind Speed at {actual_height:.1f} m AGL\nwith Building Footprints", 
                fontsize=14, fontweight='bold')
    
    # Add legend
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D([0], [0], color='red', linewidth=2, label='Building Footprints'),
        Line2D([0], [0], color='white', marker='>', markersize=8, label='Wind Direction')
    ]
    ax.legend(handles=legend_elements, loc='upper right', fontsize=11)
    
    plt.tight_layout()
    
    # Save figure
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "urban_street_canyon.png"
    plt.savefig(out_img, dpi=150, bbox_inches='tight')
    print(f"✓ Saved Urban Layout wind speed plot to: {out_img}")
    plt.close()
    
    wind.finalize()

if __name__ == '__main__':
    main()
