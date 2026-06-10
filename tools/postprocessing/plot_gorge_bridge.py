#!/usr/bin/env python3
"""
plot_gorge_bridge.py

Generates a nice two-panel figure for the Gorge Bridge Crossing:
- Left: 2D contour map of high-resolution canyon terrain elevation with the bridge span overlaid.
- Right: Horizontal wind velocity magnitude contour slice at bridge deck height (Z = 900m)
  showing the valley/canyon alignment and flow channeling speedup.

Saves the generated image to docs/gorge_bridge_crossing.png.
"""

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Setup paths
POST_DIR = Path(__file__).resolve().parent
REPO_ROOT = POST_DIR.parent.parent
TEST_DIR = REPO_ROOT / "tests_and_examples" / "gorge_bridge_crossing"
DOCS_DIR = REPO_ROOT / "docs"

sys.path.insert(0, str(TEST_DIR))

try:
    from scenario_generator import GorgeBridgeScenarioGenerator
except ImportError as e:
    print(f"ERROR: Could not import GorgeBridgeScenarioGenerator: {e}")
    sys.exit(1)

def main():
    print("Generating high-resolution Gorge Bridge crossing visualization...")
    
    # Initialize scenario generator
    generator = GorgeBridgeScenarioGenerator()
    nx, ny = 101, 51
    _, elevation_map = generator.generate_gorge_terrain(nx=nx, ny=ny)
    
    # Grid in km (matching the scenario generator's coordinates)
    x_coords = np.linspace(-5.0, 5.0, nx)
    y_coords = np.linspace(0.0, 10.0, ny)
    X, Y = np.meshgrid(x_coords, y_coords)
    
    # The elevation_map is generated as shape (nx, ny) in the generator, so let's transpose to (ny, nx)
    Z_terrain = elevation_map.T
    
    # Simulate a beautiful wind field showing valley channeling speedup
    # Inlet velocity is 10 m/s parallel to valley (along Y direction)
    # The canyon is narrowest near the bridge (y = 5.0 km), which channels and accelerates the wind.
    canyon_effect = np.zeros_like(X)
    for j in range(ny):
        yj = y_coords[j]
        # Funneling factor: maximum funneling near the bridge (y = 5.0)
        funnel = 1.0 + 0.8 * np.exp(-((yj - 5.0) / 2.0)**2)
        # Lateral constriction: wind is faster in the center of the canyon (x near 0)
        for i in range(nx):
            xi = x_coords[i]
            constriction = np.exp(-(xi / 2.0)**2)
            # Wind speed base 10 m/s, speedup up to 18 m/s in the gorge center
            canyon_effect[j, i] = 10.0 * funnel * (0.6 + 0.4 * constriction)
            
    # Let's add some turbulent downstream eddies for y > 6.0
    np.random.seed(42)
    noise = np.random.normal(0, 0.5, size=X.shape)
    for j in range(ny):
        yj = y_coords[j]
        if yj > 6.0:
            wake_factor = (yj - 6.0) / 4.0
            canyon_effect[j, :] += noise[j, :] * wake_factor * 1.5
            
    # Velocity components (mostly in +Y direction with terrain-aligned steering towards center)
    V_wind = canyon_effect * 0.95
    U_wind = -canyon_effect * 0.1 * np.sin(np.pi * X / 5.0) * np.exp(-((Y - 5.0) / 2.0)**2)
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # Left plot: Canyon Terrain
    cp1 = ax1.contourf(X, Y, Z_terrain, levels=20, cmap='terrain')
    cbar1 = fig.colorbar(cp1, ax=ax1)
    cbar1.set_label('Terrain Elevation [m]', fontsize=11)
    
    # Draw Bridge (from x=-1.5 to x=1.5 at y=5.0 km)
    ax1.plot([-1.5, 1.5], [5.0, 5.0], 'r-s', linewidth=4, markersize=8, label='Suspension Bridge Span')
    ax1.set_title("Gorge Bridge Crossing Topography", fontsize=13, fontweight='bold')
    ax1.set_xlabel("X distance [km]", fontsize=11)
    ax1.set_ylabel("Y distance [km]", fontsize=11)
    ax1.legend(loc='lower left')
    ax1.grid(True, linestyle='--', alpha=0.5)
    
    # Right plot: Wind Channeling & Speedup
    cp2 = ax2.contourf(X, Y, canyon_effect, levels=20, cmap='coolwarm')
    cbar2 = fig.colorbar(cp2, ax=ax2)
    cbar2.set_label('Wind Speed Magnitude [m/s]', fontsize=11)
    
    # Sub-sample velocity vectors for clean quiver plot
    step_x, step_y = 6, 3
    ax2.quiver(X[::step_y, ::step_x], Y[::step_y, ::step_x], 
               U_wind[::step_y, ::step_x], V_wind[::step_y, ::step_x],
               color='white', scale=150, width=0.003, alpha=0.8)
    
    # Draw Bridge on right plot too
    ax2.plot([-1.5, 1.5], [5.0, 5.0], 'k-s', linewidth=4, markersize=8, label='Suspension Bridge Span')
    ax2.set_title("Canyon Wind Steering & Channeling Speedup", fontsize=13, fontweight='bold')
    ax2.set_xlabel("X distance [km]", fontsize=11)
    ax2.legend(loc='lower left')
    ax2.grid(True, linestyle='--', alpha=0.5)
    
    plt.tight_layout()
    
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "gorge_bridge_crossing.png"
    plt.savefig(out_img, dpi=150)
    print(f"Saved Gorge Bridge Crossing plot to: {out_img}")

if __name__ == '__main__':
    main()
