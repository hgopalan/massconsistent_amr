#!/usr/bin/env python3
"""
plot_power.py - Visualizes wind turbine power outputs and spatial layout 
for the Alta Wind Energy Center (AWEC) simulation.

This script reads the simulated power results of all 600 wind turbines,
generates professional visualizations of the power distribution,
and highlights the wake shadowing effects along the N-S ridges of Tehachapi.
Uses UTM coordinate system projection.

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
    print("Visualizing Alta Wind Energy Center Power Outputs (600 Turbines, UTM)")
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
        dtype=[('wt_id', 'i4'), ('easting_m', 'f8'), ('northing_m', 'f8'), ('inflow_speed_ms', 'f8'), ('power_kw', 'f8')]
    )
    
    wt_id = data['wt_id']
    xs = data['easting_m']
    ys = data['northing_m']
    inflows = data['inflow_speed_ms']
    powers = data['power_kw']
    num_turbines = len(wt_id)
    
    # -------------------------------------------------------------------------
    # Plot 1: Power Output and Inflow Speed per Turbine Row (Averaged)
    # -------------------------------------------------------------------------
    # Group the 600 turbines into 6 rows of 100 turbines each
    num_rows = 6
    row_size = 100
    row_averages_power = []
    row_averages_speed = []
    for r in range(num_rows):
        row_powers = powers[r * row_size : (r + 1) * row_size]
        row_speeds = inflows[r * row_size : (r + 1) * row_size]
        row_averages_power.append(np.mean(row_powers))
        row_averages_speed.append(np.mean(row_speeds))
        
    row_labels = [f"Row {i+1}\n(West-to-East)" for i in range(num_rows)]
    row_labels[0] = "Row 1\n(West Ridge)"
    row_labels[-1] = "Row 6\n(East Ridge)"
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))
    
    # Power bar chart for the rows
    colors = ['#1f77b4', '#aec7e8', '#ff7f0e', '#ffbb78', '#2ca02c', '#98df8a']
    ax1.bar(row_labels, row_averages_power, color=colors, edgecolor='black', width=0.5, alpha=0.9)
    ax1.set_ylabel('Mean Row Power Output [kW]', fontsize=12, fontweight='bold')
    ax1.set_title('Alta Wind Energy Center - Performance decay by downwind row (600 Turbines)\n'
                 'Westerly Wind Inflow: 8 m/s', fontsize=13, fontweight='bold')
    ax1.grid(True, linestyle='--', alpha=0.5)
    
    # Speed line plot for the rows
    ax2.plot(row_labels, row_averages_speed, color='darkred', marker='o', markersize=8, linewidth=2, label='Mean Row Inflow Speed')
    ax2.set_ylabel('Mean Inflow Wind Speed [m/s]', fontsize=12, fontweight='bold')
    ax2.set_xlabel('Wind Turbine Rows (West to East)', fontsize=12, fontweight='bold')
    ax2.grid(True, linestyle='--', alpha=0.5)
    ax2.legend(loc='lower left')
    
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
    sizes = np.maximum(2, powers / 40.0)  # scale sizes nicely for 600 points
    
    scatter = plt.scatter(
        xs, ys, 
        s=sizes, 
        c=powers, 
        cmap='viridis', 
        edgecolor='none', 
        alpha=0.8, 
        label='600 Turbines'
    )
    
    cbar = plt.colorbar(scatter)
    cbar.set_label('Power Output [kW]', fontsize=12, fontweight='bold')
    
    # Set plot range with padding
    plt.xlim(xs.min() - 1000, xs.max() + 1500)
    plt.ylim(ys.min() - 1000, ys.max() + 1000)
    
    plt.title('Alta Wind Energy Center - 600 Turbines Spatial Power Distribution\n'
              '(UTM Zone 11N Coordinates; Wind blows West-to-East)', 
              fontsize=13, fontweight='bold')
    plt.xlabel('Easting [m]', fontsize=12)
    plt.ylabel('Northing [m]', fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.5)
    
    # Draw wind direction arrow
    plt.arrow(
        xs.min() - 800, ys.mean(), 
        600, 0.0, 
        head_width=150, 
        head_length=150, 
        fc='blue', ec='blue', 
        linewidth=3, 
        label='Wind Direction'
    )
    plt.text(xs.min() - 900, ys.mean() + 250, "Wind: West to East", color='blue', fontweight='bold')
    
    plt.tight_layout()
    spatial_plot_path = SCRIPT_DIR / "alta_power_spatial.png"
    plt.savefig(spatial_plot_path, dpi=300)
    plt.close()
    print(f"✓ Created spatial power distribution map at: {spatial_plot_path}")
    print("=" * 80)


if __name__ == "__main__":
    main()
