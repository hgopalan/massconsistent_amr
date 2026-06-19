#!/usr/bin/env python3
"""
Data Center Heat Source - Visualization and Analysis Example

This script demonstrates how to:
1. Load solver output with data center heat sources
2. Compute plume metrics for single and multiple facilities
3. Generate visualizations of horizontal and vertical slices
4. Validate against analytical Briggs plume rise
"""

import sys
import numpy as np
import matplotlib.pyplot as plt

# Try to import the datacenter analysis module
try:
    sys.path.insert(0, '/home/runner/work/massconsistent_amr/massconsistent_amr/src/python')
    from datacenter_heat_source import DataCenterPlume, DataCenterFacility
    HAS_DATACENTER_MODULE = True
except ImportError:
    HAS_DATACENTER_MODULE = False
    print("Warning: datacenter_heat_source module not available")
    print("Install with: python -m pip install .")


def create_synthetic_temperature_field():
    """
    Create synthetic temperature field for demonstration.
    
    In real usage, this would be loaded from solver output with:
    plume = DataCenterPlume.from_amrex_plotfile("plt00100")
    """
    print("\n=== Creating Synthetic Temperature Field ===")
    
    # Create a simple synthetic field
    x = np.linspace(0, 3000, 120)  # 3000m domain, 25m spacing
    y = np.linspace(0, 3000, 120)
    z = np.linspace(10, 310, 15)   # 300m height, 20m spacing
    
    X, Y, Z = np.meshgrid(x, y, z, indexing='ij')
    
    # Gaussian heat source centered at (1500, 1500, 10m)
    x_c, y_c, z_c = 1500.0, 1500.0, 10.0
    sigma_x, sigma_y, sigma_z = 100.0, 100.0, 10.0
    
    gaussian = np.exp(-((X - x_c)**2 / (2*sigma_x**2) + 
                        (Y - y_c)**2 / (2*sigma_y**2) +
                        (Z - z_c)**2 / (2*sigma_z**2)))
    
    # Temperature anomaly: simulates Gaussian heat source + some plume rise
    T_anomaly = 3.0 * gaussian  # Peak ~3K anomaly
    
    # Add simple vertical gradient from plume rise (wind advection)
    wind_speed = 10.0  # m/s
    U = np.ones_like(X) * wind_speed
    
    # Downwind dispersion (simple model)
    dx = X - x_c
    dispersion_factor = np.exp(-dx / (sigma_x * 5))  # Downwind decay
    T_anomaly *= dispersion_factor
    
    return x, y, z, T_anomaly, U


def example_single_facility_analysis():
    """Example: Analyze single data center facility."""
    print("\n" + "="*60)
    print("EXAMPLE 1: Single Facility Analysis")
    print("="*60)
    
    print("\nConfiguration:")
    print("  Facility: DataCenter_1")
    print("  Heat Release: 10 MW")
    print("  Location: (1500m, 1500m, 10m)")
    print("  Footprint: 100m × 100m")
    print("  Wind Speed: 10 m/s (neutral stratification)")
    
    # Create synthetic field
    x, y, z, T_anomaly, U = create_synthetic_temperature_field()
    
    # Compute plume statistics
    T_max = np.max(T_anomaly)
    T_mean = np.mean(T_anomaly[T_anomaly > 0.1])
    
    # Find plume rise (height where max anomaly occurs)
    max_idx = np.unravel_index(np.argmax(T_anomaly), T_anomaly.shape)
    plume_height = z[max_idx[2]]
    
    print("\nComputed Plume Metrics:")
    print(f"  Maximum Temperature Anomaly: {T_max:.3f} K")
    print(f"  Mean Temperature (plume region): {T_mean:.3f} K")
    print(f"  Plume Rise Height: {plume_height:.1f} m")
    
    # Briggs analytical formula for comparison
    heat_flux = 1.0e7  # W
    wind_speed = 10.0  # m/s
    distance = 1000.0  # m downwind
    T_ref = 300.0  # K
    
    # Briggs plume rise: dh = 1.6 * F^(1/3) * x^(2/3) / u
    g = 9.81
    dT_briggs = heat_flux / (1.225 * 1005.0 * 1000.0)  # Approximate
    F = g * (dT_briggs / T_ref) * (10.0 ** 2)  # Buoyancy parameter
    dh_briggs = 1.6 * np.power(F, 1/3) * np.power(distance, 2/3) / wind_speed
    
    print(f"\nBriggs Analytical Plume Rise (at {distance}m downwind):")
    print(f"  Predicted rise: {dh_briggs:.1f} m")
    print(f"  Temperature excess: {dT_briggs:.3f} K")
    
    return x, y, z, T_anomaly, U


def example_multi_facility_analysis():
    """Example: Analyze multiple data center facilities."""
    print("\n" + "="*60)
    print("EXAMPLE 2: Multiple Facility Analysis")
    print("="*60)
    
    facilities = [
        {"name": "DataCenter_A", "Q": 1.0e7, "x": 1000.0, "y": 1000.0},
        {"name": "DataCenter_B", "Q": 0.5e7, "x": 1500.0, "y": 2000.0},
        {"name": "DataCenter_C", "Q": 0.8e7, "x": 2500.0, "y": 1500.0},
    ]
    
    print(f"\nConfiguration ({len(facilities)} facilities):")
    total_heat = 0.0
    for f in facilities:
        print(f"  {f['name']}: {f['Q']/1e6:.1f} MW at ({f['x']:.0f}, {f['y']:.0f})")
        total_heat += f['Q']
    
    print(f"\nTotal Heat Release: {total_heat/1e6:.1f} MW")
    
    # Create synthetic combined field
    x = np.linspace(0, 3000, 120)
    y = np.linspace(0, 3000, 120)
    z = np.linspace(10, 310, 15)
    
    X, Y, Z = np.meshgrid(x, y, z, indexing='ij')
    T_combined = np.zeros_like(X)
    
    # Superposition of Gaussian sources
    for f in facilities:
        x_c, y_c, z_c = f['x'], f['y'], 10.0
        sigma = 100.0
        
        gaussian = np.exp(-((X - x_c)**2 + (Y - y_c)**2) / (2*sigma**2) -
                          (Z - z_c)**2 / (2*10**2))
        
        T_anom = (f['Q'] / (1.0e7)) * 3.0 * gaussian  # Scale by facility size
        T_combined += T_anom
    
    print(f"\nCombined Field Statistics:")
    print(f"  Maximum Temperature Anomaly: {np.max(T_combined):.3f} K")
    print(f"  Mean Temperature (plume region): {np.mean(T_combined[T_combined > 0.1]):.3f} K")
    
    # Facility-by-facility breakdown
    print(f"\nFacility Contributions to Peak Anomaly:")
    for f in facilities:
        weight = f['Q'] / total_heat
        contrib = weight * np.max(T_combined)
        print(f"  {f['name']}: {contrib:.3f} K ({weight*100:.1f}%)")
    
    return x, y, z, T_combined


def generate_visualizations(x, y, z, T_anomaly):
    """Generate example visualizations."""
    print("\n" + "="*60)
    print("EXAMPLE 3: Generating Visualizations")
    print("="*60)
    
    # Horizontal slice at mid-height
    z_idx = len(z) // 2
    z_slice = z[z_idx]
    
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    
    # Horizontal slice
    ax = axes[0]
    im = ax.contourf(x, y, T_anomaly[:, :, z_idx], levels=20, cmap='RdYlBu_r')
    ax.contour(x, y, T_anomaly[:, :, z_idx], levels=10, colors='k', alpha=0.3, linewidths=0.5)
    ax.set_xlabel('X [m]')
    ax.set_ylabel('Y [m]')
    ax.set_title(f'Horizontal Slice at z = {z_slice:.1f} m')
    ax.set_aspect('equal')
    plt.colorbar(im, ax=ax, label='ΔT [K]')
    
    # Vertical slice (downwind at x=1500m)
    x_idx = np.argmin(np.abs(x - 1500.0))
    ax = axes[1]
    im = ax.contourf(y, z, T_anomaly[x_idx, :, :].T, levels=20, cmap='RdYlBu_r')
    ax.contour(y, z, T_anomaly[x_idx, :, :].T, levels=10, colors='k', alpha=0.3, linewidths=0.5)
    ax.set_xlabel('Y [m]')
    ax.set_ylabel('Z [m]')
    ax.set_title(f'Vertical Slice at x = {x[x_idx]:.1f} m')
    plt.colorbar(im, ax=ax, label='ΔT [K]')
    
    plt.tight_layout()
    
    # Save figure
    output_file = '/tmp/datacenter_visualization.png'
    plt.savefig(output_file, dpi=100, bbox_inches='tight')
    print(f"\nVisualization saved to: {output_file}")
    
    # Also try to show if in interactive environment
    try:
        plt.show()
    except:
        pass


def main():
    """Run examples."""
    print("\n" + "="*60)
    print("Data Center Heat Source - Visualization Examples")
    print("="*60)
    
    # Example 1: Single facility
    x, y, z, T_anomaly, U = example_single_facility_analysis()
    
    # Example 2: Multiple facilities
    x, y, z, T_combined = example_multi_facility_analysis()
    
    # Example 3: Visualizations (using single facility for clarity)
    print("\nGenerating visualizations for single facility case...")
    generate_visualizations(x, y, z, T_anomaly)
    
    print("\n" + "="*60)
    print("Examples Complete")
    print("="*60)
    print("\nNext Steps:")
    print("1. Run actual solver: wind_solver_app inputs.i")
    print("2. Load output: plume = DataCenterPlume.from_amrex_plotfile('plt_xxxxx')")
    print("3. Analyze: metrics = plume.compute_plume_metrics('DataCenter_1')")
    print("4. Visualize: plume.plot_horizontal_slice(z=100.0)")
    print("\nSee docs/DATACENTER_IMPLEMENTATION.md for complete reference.")


if __name__ == "__main__":
    main()
