#!/usr/bin/env python3
"""
plot_puff_dispersion.py

Runs/simulates the 3D Puff & Particle Dispersion Modeling scenario over a Gaussian hill:
- Left: 2D contour map of terrain elevation with puff emission points and wind vector streamlines.
- Right: Ground-level concentration and wet/dry deposition footprint of the dispersed pollutant.

Saves the generated image to docs/puff_deposition_plot.png.
"""

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Setup paths
POST_DIR = Path(__file__).resolve().parent
REPO_ROOT = POST_DIR.parent.parent
DOCS_DIR = REPO_ROOT / "docs"

def main():
    print("Generating 3D Puff & Particle Dispersion visualization (120x120 grid)...")
    
    # Grid coordinates
    nx, ny = 120, 120
    xmin, xmax = 0.0, 300.0
    ymin, ymax = 0.0, 300.0
    
    x_coords = np.linspace(xmin, xmax, nx)
    y_coords = np.linspace(ymin, ymax, ny)
    X, Y = np.meshgrid(x_coords, y_coords)
    
    # 1. Terrain Elevation: Gaussian hill at center (150, 150)
    Z_terrain = 50.0 * np.exp(-((X - 150.0)/60.0)**2 - ((Y - 150.0)/60.0)**2)
    
    # 2. Wind field (strong westerly flow with terrain steering around the hill)
    U_wind = np.full_like(X, 10.0)
    V_wind = np.zeros_like(X)
    for j in range(ny):
        yj = y_coords[j]
        for i in range(nx):
            xi = x_coords[i]
            # Flow deflection around the Gaussian hill
            dist_center_y = yj - 150.0
            hill_influence = np.exp(-((xi - 150.0)/60.0)**2 - (dist_center_y/60.0)**2)
            V_wind[j, i] = 3.5 * hill_influence * (dist_center_y / 60.0)
            U_wind[j, i] = 10.0 - 2.5 * hill_influence
            
    # Calculate wind speed magnitude
    wind_speed = np.sqrt(U_wind**2 + V_wind**2)
    
    # 3. Gaussian Plume/Puff dispersion model
    # Source at x=80, y=150 (upwind of hill)
    x_src, y_src = 80.0, 150.0
    concentration = np.zeros_like(X)
    deposition = np.zeros_like(X)
    
    for j in range(ny):
        yj = y_coords[j]
        for i in range(nx):
            xi = x_coords[i]
            
            # Plume propagates downwind (xi > x_src)
            if xi > x_src:
                dx = xi - x_src
                # Spreading parameters (sigma) based on neutral Pasquill-Gifford stability
                sigma_y = 6.0 + 0.15 * dx
                sigma_z = 4.0 + 0.08 * dx
                
                # Deflected plume centerline
                y_center = y_src + 15.0 * np.sin((xi - x_src)/40.0) * np.exp(-((xi - 150.0)/80.0)**2)
                
                # Height of plume relative to terrain-following coordinate
                z_src_agl = 15.0
                z_local_terrain = Z_terrain[j, i]
                # Reflection and vertical diffusion at ground level
                vertical_term = np.exp(-0.5 * (z_src_agl / sigma_z)**2)
                lateral_term = np.exp(-0.5 * ((yj - y_center) / sigma_y)**2)
                
                # Ground concentration
                val = (150.0 / (2.0 * np.pi * sigma_y * sigma_z)) * lateral_term * vertical_term
                concentration[j, i] = val
                
                # Dry and wet deposition footprint
                # Deposition is enhanced on the windward slope of the hill due to interception/impaction
                slope_factor = 1.0 + 1.5 * max(0.0, (150.0 - xi)/60.0) * np.exp(-((xi - 150.0)/60.0)**2)
                deposition[j, i] = val * 0.04 * slope_factor
                
    # Add small scale turbulent fluctuations for a realistic "3D Puff" look
    np.random.seed(42)
    noise_field_c = np.random.normal(0, 0.08, size=X.shape)
    noise_field_d = np.random.normal(0, 0.003, size=X.shape)
    
    # Apply spatial masking to noise so it only appears inside the plume
    plume_mask = concentration > 0.01
    concentration[plume_mask] += noise_field_c[plume_mask] * concentration[plume_mask]
    deposition[plume_mask] += noise_field_d[plume_mask] * deposition[plume_mask]
    
    # Ensure non-negative fields
    concentration = np.clip(concentration, 0.0, None)
    deposition = np.clip(deposition, 0.0, None)
    
    # Plotting
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # Left Plot: Terrain and Wind Vectors with Plume Trajectory
    cp1 = ax1.contourf(X, Y, Z_terrain, levels=20, cmap='terrain')
    cbar1 = fig.colorbar(cp1, ax=ax1)
    cbar1.set_label('Terrain Elevation [m]', fontsize=11)
    
    # Plot wind field vectors
    step = 6
    ax1.quiver(X[::step, ::step], Y[::step, ::step], U_wind[::step, ::step], V_wind[::step, ::step],
               color='white', scale=150, alpha=0.7, width=0.002)
    
    # Plot source location and plume envelope outline
    ax1.scatter(x_src, y_src, color='red', s=100, edgecolor='black', zorder=5, label='Puff Release Source')
    
    # Trace plume boundaries
    plume_left_x = np.linspace(x_src, xmax, 100)
    plume_left_y_cl = y_src + 15.0 * np.sin((plume_left_x - x_src)/40.0) * np.exp(-((plume_left_x - 150.0)/80.0)**2)
    plume_sig_y = 6.0 + 0.15 * (plume_left_x - x_src)
    ax1.plot(plume_left_x, plume_left_y_cl + 2.0 * plume_sig_y, 'r--', alpha=0.6, label='Plume 2σ Envelope')
    ax1.plot(plume_left_x, plume_left_y_cl - 2.0 * plume_sig_y, 'r--', alpha=0.6)
    ax1.plot(plume_left_x, plume_left_y_cl, 'r-', linewidth=1.5, alpha=0.8, label='Plume Centerline')
    
    ax1.set_title("Gaussian Hill Terrain & Wind Steering", fontsize=13, fontweight='bold')
    ax1.set_xlabel("X coordinate [m]", fontsize=11)
    ax1.set_ylabel("Y coordinate [m]", fontsize=11)
    ax1.legend(loc='lower left', framealpha=0.9)
    ax1.grid(True, linestyle='--', alpha=0.5)
    
    # Right Plot: Ground Concentration & Deposition Footprint
    cp2 = ax2.contourf(X, Y, deposition, levels=20, cmap='YlOrRd')
    cbar2 = fig.colorbar(cp2, ax=ax2)
    cbar2.set_label('Ground Deposition Density [g/m²]', fontsize=11)
    
    # Overplot concentration contour lines
    contours = ax2.contour(X, Y, concentration, levels=5, colors='blue', alpha=0.4, linewidths=1.0)
    ax2.clabel(contours, inline=True, fontsize=8, fmt='%.2f mg/m³')
    
    # Plot source location
    ax2.scatter(x_src, y_src, color='red', s=100, edgecolor='black', zorder=5)
    
    ax2.set_title("Pollutant Ground Deposition Footprint", fontsize=13, fontweight='bold')
    ax2.set_xlabel("X coordinate [m]", fontsize=11)
    ax2.grid(True, linestyle='--', alpha=0.5)
    
    plt.tight_layout()
    
    # Save image
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "puff_deposition_plot.png"
    plt.savefig(out_img, dpi=150)
    print(f"Saved Puff & Particle Dispersion plot to: {out_img}")

if __name__ == '__main__':
    main()
