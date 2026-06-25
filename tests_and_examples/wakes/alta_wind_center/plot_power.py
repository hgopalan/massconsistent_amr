#!/usr/bin/env python3
"""
plot_power.py - Visualizes wind turbine power outputs and spatial layout 
for the Alta Wind Energy Center (AWEC) simulation.

This script reads the simulated power results of all 39 wind turbines,
generates professional visualizations of the power distribution,
and highlights the wake shadowing effects along the N-S ridges of Tehachapi.

Author: GitHub Copilot Task Agent
Date: June 25, 2026
"""

import os
import sys
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
import matplotlib.pyplot as plt

# Setup paths
SCRIPT_DIR = Path(__file__).resolve().parent
CSV_PATH = SCRIPT_DIR / "turbine_power_output.csv"


def main():
    """Reads simulated power outputs and creates plots."""
    print("=" * 80)
    print("Visualizing Alta Wind Energy Center Power Outputs")
    print("=" * 80)
    
    if not CSV_PATH.exists():
        print(f"ERROR: Simulated power outputs not found at: {CSV_PATH}")
        print("Please run the simulation first:")
        print("  python3 test_alta_wind_center.py")
        sys.exit(1)
        
    # Read the simulation data
    print(f"Loading simulated power data from: {CSV_PATH}")
    data = np.genfromtxt(
        CSV_PATH, 
        delimiter=',', 
        names=True, 
        dtype=[('wt_id', 'i4'), ('x_m', 'f8'), ('y_m', 'f8'), ('inflow_speed_ms', 'f8'), ('power_kw', 'f8')]
    )
    
    wt_id = data['wt_id']
    xs = data['x_m']
    ys = data['y_m']
    inflows = data['inflow_speed_ms']
    powers = data['power_kw']
    num_turbines = len(wt_id)
    
    # -------------------------------------------------------------------------
    # Plot 1: Power Output and Inflow Speed per Turbine
    # -------------------------------------------------------------------------
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10), sharex=True)
    
    # Define colors for different ridges to make the plot highly informative
    # West Ridge: WT 1 to 13, Central Ridge: WT 14 to 26, East Ridge: WT 27 to 39
    colors = []
    labels = []
    for i in range(num_turbines):
        if i < 13:
            colors.append('#1f77b4')  # Blue for West
            labels.append('West Ridge' if i == 0 else '')
        elif i < 26:
            colors.append('#ff7f0e')  # Orange for Central
            labels.append('Central Ridge' if i == 13 else '')
        else:
            colors.append('#2ca02c')  # Green for East
            labels.append('East Ridge' if i == 26 else '')
            
    # Power bar chart
    bars = ax1.bar(wt_id, powers, color=colors, edgecolor='black', alpha=0.8)
    ax1.set_ylabel('Turbine Power Output [kW]', fontsize=12, fontweight='bold')
    ax1.set_title('Alta Wind Energy Center - Individual Turbine Performance\n'
                 'Westerly Wind Inflow: 8 m/s', fontsize=14, fontweight='bold')
    ax1.grid(True, linestyle='--', alpha=0.5)
    
    # Inflow speed line plot
    ax2.plot(wt_id, inflows, color='darkred', marker='o', linewidth=2, label='Hub-Height Inflow Speed')
    ax2.set_ylabel('Inflow Wind Speed [m/s]', fontsize=12, fontweight='bold')
    ax2.set_xlabel('Wind Turbine ID', fontsize=12, fontweight='bold')
    ax2.set_xticks(np.arange(1, num_turbines + 1, 2))
    ax2.grid(True, linestyle='--', alpha=0.5)
    
    # Handle legends
    # Create manual legend handles for the ridges bar plot
    from matplotlib.patches import Patch
    legend_elements = [
        Patch(facecolor='#1f77b4', edgecolor='black', label='West Ridge (Upstream)'),
        Patch(facecolor='#ff7f0e', edgecolor='black', label='Central Ridge (Shadowed)'),
        Patch(facecolor='#2ca02c', edgecolor='black', label='East Ridge (Deep Shadowed)')
    ]
    ax1.legend(handles=legend_elements, loc='upper right', frameon=True)
    ax2.legend(loc='lower left', frameon=True)
    
    plt.tight_layout()
    bar_plot_path = SCRIPT_DIR / "alta_power_bars.png"
    plt.savefig(bar_plot_path, dpi=300)
    plt.close()
    print(f"✓ Created power performance chart at: {bar_plot_path}")
    
    # -------------------------------------------------------------------------
    # Plot 2: Spatial Power Layout Distribution
    # -------------------------------------------------------------------------
    plt.figure(figsize=(10, 8))
    
    # Size of the scatter points is proportional to turbine power
    sizes = np.maximum(20, powers / 10.0)  # scale sizes nicely
    
    scatter = plt.scatter(
        xs, ys, 
        s=sizes, 
        c=powers, 
        cmap='viridis', 
        edgecolor='black', 
        alpha=0.9, 
        label='Turbines'
    )
    
    cbar = plt.colorbar(scatter)
    cbar.set_label('Power Output [kW]', fontsize=12, fontweight='bold')
    
    # Add text labels for some representative turbines to show values
    for idx in [0, 6, 12, 13, 19, 25, 26, 32, 38]:
        plt.text(
            xs[idx] + 60, ys[idx] - 20, 
            f"{powers[idx]:.0f} kW", 
            fontsize=8, 
            fontweight='bold',
            bbox=dict(boxstyle="round,pad=0.2", fc="yellow", alpha=0.5, ec="gray")
        )
        
    # Set plot range with padding
    plt.xlim(xs.min() - 300, xs.max() + 500)
    plt.ylim(ys.min() - 300, ys.max() + 300)
    
    plt.title('Alta Wind Energy Center - Spatial Power Distribution\n'
              '(Circle size & color denote generated power; Wind blows West-to-East)', 
              fontsize=13, fontweight='bold')
    plt.xlabel('X Coordinate [m]', fontsize=12)
    plt.ylabel('Y Coordinate [m]', fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.5)
    
    # Draw wind direction arrow
    plt.arrow(
        xs.min() - 200, 0.0, 
        200, 0.0, 
        head_width=100, 
        head_length=100, 
        fc='blue', ec='blue', 
        linewidth=3, 
        label='Wind Direction'
    )
    plt.text(xs.min() - 250, 150, "Wind: West to East", color='blue', fontweight='bold')
    
    plt.tight_layout()
    spatial_plot_path = SCRIPT_DIR / "alta_power_spatial.png"
    plt.savefig(spatial_plot_path, dpi=300)
    plt.close()
    print(f"✓ Created spatial power distribution map at: {spatial_plot_path}")
    print("=" * 80)


if __name__ == "__main__":
    main()
