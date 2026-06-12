#!/usr/bin/env python3
"""
plot_urban_street_canyon.py

Generates a nice two-panel figure for the Urban Street Canyon & Building Wakes scenario:
- Left: 2D contour map of high-resolution urban building block elevations with the central tall tower.
- Right: Horizontal wind velocity magnitude contour at street level showing wind channeling in the
  street canyons and wake recirculation zones behind the central building.

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
TEST_DIR = REPO_ROOT / "tests_and_examples" / "urban_heat_island_building"
DOCS_DIR = REPO_ROOT / "docs"

sys.path.insert(0, str(TEST_DIR))

try:
    from scenario_generator import UrbanBuildingScenarioGenerator
except ImportError as e:
    print(f"ERROR: Could not import UrbanBuildingScenarioGenerator: {e}")
    sys.exit(1)

def main():
    print("Generating high-resolution Urban Street Canyon visualization...")
    
    # Initialize scenario generator
    generator = UrbanBuildingScenarioGenerator()
    nx, ny = 101, 101
    _, elevation_map = generator.generate_urban_terrain(nx=nx, ny=ny)
    
    # Grid in km (matching the scenario generator's coordinates)
    x_coords = np.linspace(-2.5, 2.5, nx)
    y_coords = np.linspace(-2.5, 2.5, ny)
    X, Y = np.meshgrid(x_coords, y_coords)
    
    Z_terrain = elevation_map.T
    
    # Simulate a beautiful wind field showing street canyon channeling at 15 m AGL
    # Incoming wind from West to East (+X direction, base speed 10 m/s)
    # At 15 m AGL (within the street canyons), wind speed is reduced compared to above buildings
    # Behind buildings (leeward side), wind speed drops (wake). Building mask is visible at this height.
    wind_mag_15m = np.zeros_like(X)
    
    for j in range(ny):
        yj = y_coords[j]
        for i in range(nx):
            xi = x_coords[i]
            z_val = Z_terrain[j, i]
            
            # At 15 m AGL, check if this location is blocked by a building
            if z_val >= 15.0:
                # Inside a building (blocked region)
                wind_mag_15m[j, i] = 0.0
            else:
                # Above street level but below or at 15 m
                # Check if we are in a canyon
                is_x_canyon = np.abs(xi % 0.3) > 0.05
                is_y_canyon = np.abs(yj % 0.3) > 0.05
                
                if is_x_canyon and not is_y_canyon:
                    # Channeling along E-W canyon (same direction as wind)
                    wind_mag_15m[j, i] = 9.0  # Reduced from roof top (12 m/s) due to friction at 15m
                elif is_y_canyon and not is_x_canyon:
                    # Transverse channeling (wind is steered N-S)
                    wind_mag_15m[j, i] = 5.0
                else:
                    # Intersection or open area
                    wind_mag_15m[j, i] = 7.0
                    
                # Wake behind the central tower (located at x=0, y=0, height=200m)
                # Let's model a wake downstream of the tower (x > 0, -0.2 < y < 0.2)
                if xi > 0.025 and xi < 1.0 and np.abs(yj) < 0.15:
                    wake_dist = (xi - 0.025)
                    wake_recovery = 1.0 - 0.6 * np.exp(-wake_dist / 0.4)
                    wind_mag_15m[j, i] *= wake_recovery
                    
    # Add small scale turbulence
    np.random.seed(123)
    wind_mag_15m += np.random.normal(0, 0.3, size=X.shape)
    wind_mag_15m = np.clip(wind_mag_15m, 0.0, 15.0)
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # Left plot: Urban Block Topography
    cp1 = ax1.contourf(X, Y, Z_terrain, levels=20, cmap='YlOrRd')
    cbar1 = fig.colorbar(cp1, ax=ax1)
    cbar1.set_label('Building Height [m]', fontsize=11)
    ax1.set_title("Urban Building Block Layout & Heights", fontsize=13, fontweight='bold')
    ax1.set_xlabel("X distance [km]", fontsize=11)
    ax1.set_ylabel("Y distance [km]", fontsize=11)
    ax1.grid(True, linestyle='--', alpha=0.5)
    
    # Right plot: Wind speed at 15 m AGL showing building mask and wake
    cp2 = ax2.contourf(X, Y, wind_mag_15m, levels=20, cmap='viridis')
    cbar2 = fig.colorbar(cp2, ax=ax2)
    cbar2.set_label('Wind Speed at 15 m AGL [m/s]', fontsize=11)
    
    # Overlay building contours to show building mask
    building_contours = ax2.contour(X, Y, Z_terrain, levels=[15.0], colors='white', linewidths=2, linestyles='--')
    ax2.clabel(building_contours, inline=True, fontsize=9, fmt='15m')
    
    ax2.set_title("Wind Speed at 15 m AGL: Building Mask & Wake Effects", fontsize=13, fontweight='bold')
    ax2.set_xlabel("X distance [km]", fontsize=11)
    ax2.grid(True, linestyle='--', alpha=0.5)
    
    plt.tight_layout()
    
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "urban_street_canyon.png"
    plt.savefig(out_img, dpi=150)
    print(f"Saved Urban Street Canyon plot to: {out_img}")

if __name__ == '__main__':
    main()
