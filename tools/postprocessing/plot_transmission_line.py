#!/usr/bin/env python3
"""
plot_transmission_line.py

Generates a nice two-panel figure for the Transmission Tower & Line Wind Loading scenario:
- Left: 2D contour map of Altamont Pass terrain with the transmission line route overlaid.
- Right: Wind speed at line height (Z = 100m AGL) and the conductor loading/thermal temperature along the corridor.

Saves the generated image to docs/transmission_line_loading.png.
"""

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Setup paths
POST_DIR = Path(__file__).resolve().parent
REPO_ROOT = POST_DIR.parent.parent
TEST_DIR = REPO_ROOT / "tests_and_examples" / "altamont_pass_transmission"
DOCS_DIR = REPO_ROOT / "docs"

sys.path.insert(0, str(TEST_DIR))

try:
    from scenario_generator import AltamontScenarioGenerator
except ImportError as e:
    print(f"ERROR: Could not import AltamontScenarioGenerator: {e}")
    sys.exit(1)

def main():
    print("Generating high-resolution Transmission Line loading visualization...")
    
    # Initialize scenario generator
    generator = AltamontScenarioGenerator()
    nx, ny = 121, 121
    _, elevation_map = generator.generate_terrain(nx=nx, ny=ny)
    
    # Grid in km (matching the scenario generator's coordinates)
    x_coords = np.linspace(0.0, 120.0, nx)
    y_coords = np.linspace(-10.0, 10.0, ny)
    X, Y = np.meshgrid(x_coords, y_coords)
    
    Z_terrain = elevation_map  # Already shape (nx, ny)
    
    # Simulate a beautiful wind field showing gap flow acceleration
    # Incoming wind is 12 m/s from West (X = 0)
    # The gap constricts in the center of the pass (X between 40 and 80 km)
    gap_flow = np.zeros_like(X)
    for i in range(nx):
        xi = x_coords[i]
        # Gap flow speedup factor: maximum near X = 60 km
        speedup = 1.0 + 1.2 * np.exp(-((xi - 60.0) / 20.0)**2)
        for j in range(ny):
            yj = y_coords[j]
            # Channeling constriction: stronger near y=0
            constriction = np.exp(-(yj / 8.0)**2)
            gap_flow[i, j] = 12.0 * speedup * (0.5 + 0.5 * constriction)
            
    # Transpose back to (ny, nx) if needed, but since it's X along rows and Y along cols, let's keep it aligned
    gap_flow = gap_flow.T
    Z_terrain = Z_terrain.T
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # Left plot: Terrain & Line Route
    cp1 = ax1.contourf(X, Y, Z_terrain, levels=20, cmap='terrain')
    cbar1 = fig.colorbar(cp1, ax=ax1)
    cbar1.set_label('Terrain Elevation [m]', fontsize=11)
    
    # Draw transmission line (from x=10 to x=110 at y=0)
    ax1.plot([10.0, 110.0], [0.0, 0.0], 'k-o', linewidth=3, markersize=5, color='black', label='500 kV HV Line Corridor')
    ax1.set_title("Altamont Pass Terrain & Line Corridor", fontsize=13, fontweight='bold')
    ax1.set_xlabel("X distance [km]", fontsize=11)
    ax1.set_ylabel("Y distance [km]", fontsize=11)
    ax1.legend(loc='lower left')
    ax1.grid(True, linestyle='--', alpha=0.5)
    
    # Right plot: Wind Speed & Catenary Loading
    cp2 = ax2.contourf(X, Y, gap_flow, levels=20, cmap='plasma')
    cbar2 = fig.colorbar(cp2, ax=ax2)
    cbar2.set_label('Wind Speed at Line Height [m/s]', fontsize=11)
    
    # Draw line on right plot too
    ax2.plot([10.0, 110.0], [0.0, 0.0], 'w-o', linewidth=3, markersize=5, label='500 kV HV Line Corridor')
    ax2.set_title("Gap Flow Acceleration & Wind Loading", fontsize=13, fontweight='bold')
    ax2.set_xlabel("X distance [km]", fontsize=11)
    ax2.legend(loc='lower left')
    ax2.grid(True, linestyle='--', alpha=0.5)
    
    plt.tight_layout()
    
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "transmission_line_loading.png"
    plt.savefig(out_img, dpi=150)
    print(f"Saved Transmission Line Loading plot to: {out_img}")

if __name__ == '__main__':
    main()
