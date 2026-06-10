#!/usr/bin/env python3
"""
plot_turbine_wake.py

Generates a nice two-panel figure for the Yawed Wind Turbine Wake Deflection scenario:
- Left: Horizontal slice (X-Y) of wind velocity deficit behind two turbines with 0 degree yaw (no deflection).
- Right: Horizontal slice (X-Y) of wind velocity deficit behind two turbines with 25 degree yaw,
  showing the yaw-induced lateral wake deflection (Bastankhah deflection).

Saves the generated image to docs/turbine_wake_deflection.png.
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
    print("Generating high-resolution Wind Turbine Wake Deflection visualization...")
    
    # Let's create a beautiful analytical plot of Bastankhah & Porté-Agel wake model
    nx, ny = 200, 100
    x_coords = np.linspace(0.0, 500.0, nx)
    y_coords = np.linspace(-100.0, 100.0, ny)
    X, Y = np.meshgrid(x_coords, y_coords)
    
    # Turbine properties
    D = 126.0  # Rotor diameter (NREL 5MW)
    U_inf = 10.0  # Free stream velocity
    
    # Helper to calculate analytical wake deficit (Bastankhah model)
    def calculate_wake(X_grid, Y_grid, yaw_deg=0.0):
        # Base velocity is free stream
        U = np.full_like(X_grid, U_inf)
        
        # Turbine 1 is located at (50.0, 0.0)
        x_t1, y_t1 = 50.0, 0.0
        
        # Wake expansion parameters
        k_w = 0.03
        ct = 0.8
        
        yaw_rad = np.radians(yaw_deg)
        
        # Calculate wake behind Turbine 1
        for j in range(ny):
            for i in range(nx):
                x = X_grid[j, i]
                y = Y_grid[j, i]
                
                # Check downstream of T1
                if x > x_t1:
                    dx = x - x_t1
                    # Wake width
                    sigma = k_w * dx + D / np.sqrt(8.0)
                    
                    # Deflection
                    if yaw_deg != 0.0:
                        # Bastankhah analytical deflection formula approximation
                        theta = 0.3 * yaw_rad * (1.0 - np.sqrt(1.0 - ct))
                        deflection = theta * dx
                    else:
                        deflection = 0.0
                        
                    # Centerline deficit
                    center_deficit = U_inf * (1.0 - np.sqrt(1.0 - ct / (8.0 * (sigma / D)**2)))
                    # Radial distribution
                    r_sq = (y - y_t1 - deflection)**2
                    deficit = center_deficit * np.exp(-r_sq / (2.0 * sigma**2))
                    
                    U[j, i] -= deficit
                    
        # Clip minimum velocity to be physical
        return np.clip(U, 1.0, U_inf)
        
    # Calculate wake fields
    U_no_yaw = calculate_wake(X, Y, yaw_deg=0.0)
    U_yaw = calculate_wake(X, Y, yaw_deg=25.0)
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    
    # Plot 1: No Yaw
    cp1 = ax1.contourf(X, Y, U_no_yaw, levels=30, cmap='viridis')
    cbar1 = fig.colorbar(cp1, ax=ax1, orientation='vertical', aspect=15)
    cbar1.set_label('Wind Velocity [m/s]', fontsize=11)
    
    # Draw Turbines
    ax1.plot([50.0, 50.0], [-D/2.0, D/2.0], 'k-', linewidth=4, label='Turbine Rotor')
    ax1.plot([250.0, 250.0], [-D/2.0, D/2.0], 'k--', linewidth=4, label='Downstream Turbine')
    ax1.set_title("Standard Wake Deficit (0° Yaw) — High Downstream Inflow Deficit", fontsize=13, fontweight='bold')
    ax1.set_ylabel("Y position [m]", fontsize=11)
    ax1.legend(loc='lower left')
    ax1.grid(True, linestyle='--', alpha=0.3)
    
    # Plot 2: Yawed (25° Yaw)
    cp2 = ax2.contourf(X, Y, U_yaw, levels=30, cmap='viridis')
    cbar2 = fig.colorbar(cp2, ax=ax2, orientation='vertical', aspect=15)
    cbar2.set_label('Wind Velocity [m/s]', fontsize=11)
    
    # Draw Yawed Turbine
    yaw_length = D / 2.0
    dx_yaw = yaw_length * np.sin(np.radians(25.0))
    dy_yaw = yaw_length * np.cos(np.radians(25.0))
    ax2.plot([50.0 - dx_yaw, 50.0 + dx_yaw], [-dy_yaw, dy_yaw], 'k-', linewidth=4, label='Yawed Rotor (25°)')
    ax2.plot([250.0, 250.0], [-D/2.0, D/2.0], 'k--', linewidth=4, label='Downstream Turbine')
    
    ax2.set_title("Deflected Wake Deficit (25° Yaw) — Deflected Away from Downstream Inflow", fontsize=13, fontweight='bold')
    ax2.set_xlabel("Downstream X distance [m]", fontsize=11)
    ax2.set_ylabel("Y position [m]", fontsize=11)
    ax2.legend(loc='lower left')
    ax2.grid(True, linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "turbine_wake_deflection.png"
    plt.savefig(out_img, dpi=150)
    print(f"Saved Turbine Wake Deflection plot to: {out_img}")

if __name__ == '__main__':
    main()
