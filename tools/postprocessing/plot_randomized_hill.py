#!/usr/bin/env python3
"""
plot_randomized_hill.py

Generates a visualization of the Randomized Hill with turbines scenario:
- Shows the randomized hill terrain profile
- Displays placed wind turbines
- Includes wind field or height visualization

Saves the generated image to docs/randomized_hill_simulation.png.
"""

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Setup paths
POST_DIR = Path(__file__).resolve().parent
REPO_ROOT = POST_DIR.parent.parent
TEST_DIR = REPO_ROOT / "tests_and_examples" / "randomized_hill_turbines"
DOCS_DIR = REPO_ROOT / "docs"

def generate_randomized_terrain(nx=21, ny=21, domain_x=1000.0, domain_y=1000.0):
    """Generate randomized hill terrain matching the test case."""
    np.random.seed(42)
    dx = domain_x / (nx - 1)
    dy = domain_y / (ny - 1)
    xc, yc = domain_x / 2.0, domain_y / 2.0
    peak_height = 100.0
    sigma = 150.0
    
    terrain_data = np.zeros((ny, nx))
    x_coords = np.zeros(nx)
    y_coords = np.zeros(ny)
    
    for j in range(ny):
        y = j * dy
        y_coords[j] = y
        for i in range(nx):
            x = i * dx
            if j == 0:
                x_coords[i] = x
            
            # Base Gaussian hill
            r_squared = (x - xc)**2 + (y - yc)**2
            base_z = peak_height * np.exp(-r_squared / (2.0 * sigma**2))
            
            # Taper near boundaries to ensure perfectly flat edges at z=0
            dist_x = min(x, domain_x - x) / 200.0
            dist_y = min(y, domain_y - y) / 200.0
            taper = min(1.0, max(0.0, dist_x)) * min(1.0, max(0.0, dist_y))
            
            # Seeded noise for randomized hill profile
            noise = np.random.uniform(-5.0, 5.0) * taper
            z = max(0.0, base_z + noise)
            terrain_data[j, i] = z
    
    return terrain_data, x_coords, y_coords

def load_turbines(filename):
    """Load turbine positions from CSV."""
    turbines = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            # Parse: x, y, hub_height, rotor_diameter, default_ct, power_curve_file
            parts = [p.strip() for p in line.split(',')]
            if len(parts) >= 2:
                x = float(parts[0])
                y = float(parts[1])
                turbines.append({'x': x, 'y': y})
    return turbines

def main():
    print("Generating Randomized Hill Simulation visualization...")
    
    # Change to test directory
    os.chdir(TEST_DIR)
    
    # Generate terrain
    terrain, x_coords, y_coords = generate_randomized_terrain()
    X, Y = np.meshgrid(x_coords, y_coords)
    
    # Load turbines
    turbines = []
    if os.path.exists("turbines.csv"):
        turbines = load_turbines("turbines.csv")
    
    # Create figure with terrain
    fig, ax = plt.subplots(figsize=(12, 10))
    
    # Plot terrain height contours
    contour_levels = np.linspace(0, terrain.max(), 15)
    cf = ax.contourf(X, Y, terrain, levels=contour_levels, cmap='terrain', alpha=0.8)
    cbar = fig.colorbar(cf, ax=ax)
    cbar.set_label('Terrain Elevation [m]', fontsize=11)
    
    # Add contour lines
    contour_lines = ax.contour(X, Y, terrain, levels=contour_levels, 
                               colors='black', alpha=0.3, linewidths=0.5)
    ax.clabel(contour_lines, inline=True, fontsize=8, fmt='%1.0f')
    
    # Plot turbine locations
    if turbines:
        tx = [t['x'] for t in turbines]
        ty = [t['y'] for t in turbines]
        ax.scatter(tx, ty, c='red', s=200, marker='^', 
                  edgecolors='darkred', linewidths=2, 
                  label=f'Wind Turbines (n={len(turbines)})', zorder=5)
        
        # Add turbine labels
        for i, turbine in enumerate(turbines):
            ax.annotate(f'{i+1}', xy=(turbine['x'], turbine['y']),
                       xytext=(3, 3), textcoords='offset points',
                       fontsize=7, alpha=0.7)
    
    # Set axis properties
    ax.set_xlabel("X distance [m]", fontsize=12)
    ax.set_ylabel("Y distance [m]", fontsize=12)
    ax.set_title("Randomized Hill Simulation with Wind Turbine Array", 
                fontsize=14, fontweight='bold')
    ax.grid(True, linestyle='--', alpha=0.3)
    ax.set_aspect('equal')
    
    if turbines:
        ax.legend(loc='upper right', fontsize=11)
    
    plt.tight_layout()
    
    # Save figure
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "randomized_hill_simulation.png"
    plt.savefig(out_img, dpi=150, bbox_inches='tight')
    print(f"✓ Saved Randomized Hill Simulation plot to: {out_img}")
    plt.close()

if __name__ == '__main__':
    main()
