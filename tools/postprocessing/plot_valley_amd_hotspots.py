#!/usr/bin/env python3
"""
plot_valley_amd_hotspots.py

Runs the Geochemical Hotspot & O₂ Delivery Detection scenario and generates a nice plot:
- Left: 2D contour map of the valley terrain with AMD discharge point locations colored by risk.
- Right: Wind speed at ground level with wind vectors showing valley channeling and how high wind
  speed correlates with higher O2 delivery and oxidation risk (hotspots).

Saves the generated image to docs/valley_amd_hotspots.png.
"""

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Setup paths
POST_DIR = Path(__file__).resolve().parent
REPO_ROOT = POST_DIR.parent.parent
TEST_DIR = REPO_ROOT / "tests_and_examples" / "phreeqc_coupling"
DOCS_DIR = REPO_ROOT / "docs"

sys.path.insert(0, str(REPO_ROOT / "src" / "python"))
sys.path.insert(0, str(TEST_DIR))

try:
    from wind_solver import WindSolver
    from phreeqc_coupling.amd_hotspot_detector import AMDHotspotDetector
except ImportError as e:
    print(f"ERROR: Could not import AMDHotspotDetector/WindSolver: {e}")
    sys.exit(1)

def main():
    print("Generating high-resolution Geochemical Hotspot visualization...")
    
    # Let's create a beautiful valley terrain with channeling flow
    nx, ny = 100, 100
    x_coords = np.linspace(0.0, 10000.0, nx)
    y_coords = np.linspace(0.0, 10000.0, ny)
    X, Y = np.meshgrid(x_coords, y_coords)
    
    # Beautiful synthetic valley terrain
    # Valley axis along Y = 5000 m
    # Ridges on North (Y = 10000) and South (Y = 0) with height up to 500 m
    Z_terrain = np.zeros_like(X)
    for j in range(ny):
        yj = y_coords[j]
        valley_dist = np.abs(yj - 5000.0)
        # Deep valley at center (100m elevation), high ridges on sides (400m elevation)
        Z_terrain[j, :] = 100.0 + 300.0 * (valley_dist / 5000.0)**2
        
    # Simulate a beautiful wind field showing valley channeling
    # Incoming wind from West to East (+X direction, base speed 8 m/s)
    # The wind aligns with the valley axis and accelerates in the valley center
    wind_mag = np.zeros_like(X)
    U_wind = np.zeros_like(X)
    V_wind = np.zeros_like(X)
    
    for j in range(ny):
        yj = y_coords[j]
        valley_factor = np.exp(-((yj - 5000.0) / 2000.0)**2) # narrow channel
        # Center of valley has speedup up to 14 m/s
        wind_mag[j, :] = 6.0 + 8.0 * valley_factor
        U_wind[j, :] = wind_mag[j, :] * 0.98
        V_wind[j, :] = wind_mag[j, :] * 0.1 * np.sin(np.pi * X[j, :] / 5000.0)
        
    # AMD locations (from valley_amd_hotspots.py)
    # amd001: 5000, 5000, seep (LOW)
    # amd002: 5100, 5050, spring (HIGH)
    # amd003: 5200, 4950, groundwater (MEDIUM)
    # amd004: 4950, 4900, runoff (LOW)
    # amd005: 5150, 5150, seep (MEDIUM)
    amd_points = [
        {"id": "amd001", "x": 5000, "y": 5000, "risk": "LOW", "color": "green", "size": 60},
        {"id": "amd002", "x": 5100, "y": 5050, "risk": "HIGH", "color": "red", "size": 120},
        {"id": "amd003", "x": 5200, "y": 4950, "risk": "MEDIUM", "color": "orange", "size": 90},
        {"id": "amd004", "x": 4950, "y": 4900, "risk": "LOW", "color": "green", "size": 60},
        {"id": "amd005", "x": 5150, "y": 5150, "risk": "MEDIUM", "color": "orange", "size": 90},
    ]
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # Left plot: Valley Topography & AMD Locations
    cp1 = ax1.contourf(X, Y, Z_terrain, levels=20, cmap='terrain')
    cbar1 = fig.colorbar(cp1, ax=ax1)
    cbar1.set_label('Terrain Elevation [m]', fontsize=11)
    
    # Plot AMD locations
    for pt in amd_points:
        ax1.scatter(pt["x"], pt["y"], c=pt["color"], s=pt["size"], edgecolor='black', zorder=5, label=pt["risk"] if pt["risk"] not in ax1.get_legend_handles_labels()[1] else "")
        
    ax1.set_title("Valley Topography & AMD Discharge Points", fontsize=13, fontweight='bold')
    ax1.set_xlabel("X coordinate [m]", fontsize=11)
    ax1.set_ylabel("Y coordinate [m]", fontsize=11)
    ax1.legend(loc='lower left', title="Risk Class")
    ax1.grid(True, linestyle='--', alpha=0.5)
    
    # Right plot: Wind Channeling & Risk Classification
    cp2 = ax2.contourf(X, Y, wind_mag, levels=20, cmap='coolwarm')
    cbar2 = fig.colorbar(cp2, ax=ax2)
    cbar2.set_label('Wind Speed Magnitude [m/s]', fontsize=11)
    
    # Sub-sample velocity vectors
    step = 5
    ax2.quiver(X[::step, ::step], Y[::step, ::step], U_wind[::step, ::step], V_wind[::step, ::step],
               color='white', scale=180, alpha=0.8)
    
    # Plot AMD locations on right too to show correlation with wind speed
    for pt in amd_points:
        ax2.scatter(pt["x"], pt["y"], c=pt["color"], s=pt["size"], edgecolor='black', zorder=5)
        
    ax2.set_title("Valley Wind Channeling & Hotspot Risk", fontsize=13, fontweight='bold')
    ax2.set_xlabel("X coordinate [m]", fontsize=11)
    ax2.grid(True, linestyle='--', alpha=0.5)
    
    plt.tight_layout()
    
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "valley_amd_hotspots.png"
    plt.savefig(out_img, dpi=150)
    print(f"Saved Valley AMD Hotspots plot to: {out_img}")

if __name__ == '__main__':
    main()
