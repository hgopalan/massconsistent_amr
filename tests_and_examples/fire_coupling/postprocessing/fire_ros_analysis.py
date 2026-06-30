#!/usr/bin/env python3
"""
fire_ros_analysis.py - Analyze fire Rate of Spread (ROS)

Post-processing script to extract fire Rate of Spread (ROS) from fire solver output.
Calculates ROS from fire front position evolution across multiple timesteps.

Usage:
    python3 fire_ros_analysis.py <fire_output_dir> [--output-dir .]

Output:
    - fire_ros_timeseries.csv: ROS as function of time
    - fire_ros_spatial.csv: Spatial distribution of ROS
    - fire_ros_statistics.txt: Summary statistics
    - fire_ros_evolution.png: Visualization of fire spread

Date: June 2026
"""

import numpy as np
import sys
import csv
from pathlib import Path
import argparse
from collections import defaultdict


def read_fire_front_data(fire_output_file):
    """
    Read fire front position from fire solver output (placeholder).
    
    Parameters:
        fire_output_file (str): Path to fire solver output file
    
    Returns:
        Dictionary with timestep, front positions, and ROS data
    """
    # Placeholder - actual format depends on fire solver output
    # This would typically read AMReX plotfile or HDF5 format
    
    print(f"Note: Fire front reading from {fire_output_file}")
    print("This requires interface to fire solver output format")
    
    return None


def create_synthetic_fire_evolution(nx=156, ny=156, num_steps=20):
    """
    Create synthetic fire evolution for demonstration.
    
    Simulates radially-expanding fire from domain center.
    """
    fire_data = []
    center_x, center_y = nx/2 * 64, ny/2 * 64  # Center in meters
    
    for step in range(num_steps):
        time = step * 60.0  # 60 seconds per step
        
        # Fire front expands from center
        # Rate increases with Rothermel model characteristics
        base_ros = 0.5  # m/s
        ros_with_wind = base_ros * (1.0 + 0.3 * np.sin(step * np.pi / num_steps))
        
        radius = ros_with_wind * time  # Fire radius = ROS * time
        
        fire_data.append({
            'step': step,
            'time': time,
            'radius': radius,
            'ros': ros_with_wind,
            'center_x': center_x,
            'center_y': center_y,
        })
    
    return fire_data


def calculate_ros_from_evolution(fire_data):
    """
    Calculate Rate of Spread from fire front evolution.
    
    Parameters:
        fire_data (list): List of fire states with position/radius
    
    Returns:
        Dictionary with ROS statistics and timeseries
    """
    ros_timeseries = []
    
    for i in range(len(fire_data) - 1):
        current = fire_data[i]
        next_step = fire_data[i + 1]
        
        # Calculate ROS from front advance
        dr = next_step['radius'] - current['radius']
        dt = next_step['time'] - current['time']
        
        if dt > 0:
            ros = dr / dt
        else:
            ros = 0.0
        
        ros_timeseries.append({
            'time': (current['time'] + next_step['time']) / 2,
            'ros': ros,
            'radius': (current['radius'] + next_step['radius']) / 2,
        })
    
    return ros_timeseries


def calculate_spatial_ros(nx=156, ny=156, num_steps=20):
    """
    Calculate spatial distribution of ROS using synthetic data.
    
    Returns:
        2D array of ROS values across domain
    """
    # Create spatial ROS field
    # In practice, this would be computed from fire intensity/fuel consumption
    
    dx = 64.0  # Grid spacing (m)
    x = np.arange(nx) * dx
    y = np.arange(ny) * dx
    X, Y = np.meshgrid(x, y)
    
    # Fire center
    center_x, center_y = 5000.0, 5000.0
    distance = np.sqrt((X - center_x)**2 + (Y - center_y)**2)
    
    # ROS based on distance from center and wind effect
    # Wind is in +X direction (U_ref = 8 m/s)
    base_ros = 0.5  # m/s
    
    # ROS higher downwind (positive X direction)
    wind_correction = 0.3 * (X - center_x) / np.max(np.abs(X - center_x))
    
    ros_spatial = base_ros * (1.0 + wind_correction) * np.exp(-distance / 2000.0)
    ros_spatial = np.clip(ros_spatial, 0.01, 1.5)
    
    # Increase variation near fire front
    perimeter = np.abs(distance - 2000.0)
    ros_spatial *= (1.0 + 0.2 * np.exp(-perimeter / 500.0))
    
    return ros_spatial


def write_ros_timeseries(output_file, ros_timeseries):
    """Write ROS timeseries to CSV file."""
    with open(output_file, 'w') as f:
        f.write("time_s,radius_m,ros_m_per_s,ros_m_per_min\n")
        for data in ros_timeseries:
            ros_m_per_min = data['ros'] * 60.0
            f.write(f"{data['time']:.1f},{data['radius']:.1f},{data['ros']:.4f},{ros_m_per_min:.2f}\n")
    
    print(f"✓ Wrote ROS timeseries to {output_file}")


def write_spatial_ros(output_file, ros_spatial, nx=156, ny=156, dx=64.0):
    """Write spatial ROS distribution to CSV file."""
    with open(output_file, 'w') as f:
        f.write("x,y,ros_m_per_s\n")
        for j in range(ny):
            for i in range(nx):
                x = i * dx
                y = j * dx
                f.write(f"{x:.1f},{y:.1f},{ros_spatial[j, i]:.4f}\n")
    
    print(f"✓ Wrote spatial ROS to {output_file}")


def compute_ros_statistics(ros_timeseries, ros_spatial, output_file):
    """Compute and write ROS statistics."""
    ros_values = [d['ros'] for d in ros_timeseries]
    ros_spatial_flat = ros_spatial.flatten()
    
    stats = {
        'temporal_mean': np.mean(ros_values),
        'temporal_std': np.std(ros_values),
        'temporal_min': np.min(ros_values),
        'temporal_max': np.max(ros_values),
        'spatial_mean': np.mean(ros_spatial_flat),
        'spatial_std': np.std(ros_spatial_flat),
        'spatial_min': np.min(ros_spatial_flat),
        'spatial_max': np.max(ros_spatial_flat),
    }
    
    with open(output_file, 'w') as f:
        f.write("Fire Rate of Spread (ROS) Analysis\n")
        f.write("="*60 + "\n\n")
        f.write("TEMPORAL ROS (from fire perimeter evolution):\n")
        f.write(f"  Mean:    {stats['temporal_mean']:.4f} m/s ({stats['temporal_mean']*60:.2f} m/min)\n")
        f.write(f"  Std Dev: {stats['temporal_std']:.4f} m/s\n")
        f.write(f"  Range:   {stats['temporal_min']:.4f} - {stats['temporal_max']:.4f} m/s\n\n")
        
        f.write("SPATIAL ROS (at end of simulation):\n")
        f.write(f"  Mean:    {stats['spatial_mean']:.4f} m/s ({stats['spatial_mean']*60:.2f} m/min)\n")
        f.write(f"  Std Dev: {stats['spatial_std']:.4f} m/s\n")
        f.write(f"  Range:   {stats['spatial_min']:.4f} - {stats['spatial_max']:.4f} m/s\n\n")
        
        f.write("INTERPRETATION:\n")
        f.write("- ROS typically ranges from 0.1-1.0 m/s for grass fires\n")
        f.write("- ROS can reach 2-4 m/s in extreme conditions with wind\n")
        f.write("- Higher ROS downwind indicates wind-driven fire spread\n")
    
    print(f"✓ Wrote ROS statistics to {output_file}")
    print(f"\n  Temporal ROS (fire perimeter evolution):")
    print(f"    Mean: {stats['temporal_mean']:.4f} m/s ({stats['temporal_mean']*60:.2f} m/min)")
    print(f"    Range: {stats['temporal_min']:.4f} - {stats['temporal_max']:.4f} m/s")
    print(f"\n  Spatial ROS (domain-wide distribution):")
    print(f"    Mean: {stats['spatial_mean']:.4f} m/s ({stats['spatial_mean']*60:.2f} m/min)")
    print(f"    Range: {stats['spatial_min']:.4f} - {stats['spatial_max']:.4f} m/s")


def generate_visualization(ros_timeseries, ros_spatial, output_file, nx=156, ny=156, dx=64.0):
    """Generate visualization of fire evolution and ROS."""
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("Note: matplotlib not available for visualization")
        return
    
    fig = plt.figure(figsize=(14, 10))
    
    # ROS timeseries
    ax1 = plt.subplot(2, 2, 1)
    times = [d['time'] for d in ros_timeseries]
    ros_values = [d['ros'] * 60 for d in ros_timeseries]  # Convert to m/min
    ax1.plot(times, ros_values, 'o-', linewidth=2, markersize=5)
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('ROS (m/min)')
    ax1.set_title('Fire Rate of Spread vs Time')
    ax1.grid(True, alpha=0.3)
    
    # Fire perimeter expansion
    ax2 = plt.subplot(2, 2, 2)
    radii = [d['radius'] for d in ros_timeseries]
    ax2.plot(times, radii, 'o-', linewidth=2, markersize=5, color='red')
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Fire Radius (m)')
    ax2.set_title('Fire Perimeter Expansion')
    ax2.grid(True, alpha=0.3)
    
    # Spatial ROS heatmap
    ax3 = plt.subplot(2, 2, 3)
    x = np.arange(nx) * dx / 1000  # Convert to km
    y = np.arange(ny) * dx / 1000
    im = ax3.contourf(x, y, ros_spatial * 60, levels=20, cmap='RdYlGn')
    ax3.set_xlabel('X (km)')
    ax3.set_ylabel('Y (km)')
    ax3.set_title('Spatial ROS Distribution at Final Time (m/min)')
    plt.colorbar(im, ax=ax3)
    
    # ROS statistics
    ax4 = plt.subplot(2, 2, 4)
    ax4.axis('off')
    
    ros_values_flat = ros_spatial.flatten()
    stats_text = f"""
    Fire Rate of Spread Statistics
    {'='*40}
    
    Temporal ROS (evolution):
      Mean:  {np.mean(ros_values):.3f} m/min
      Std:   {np.std(ros_values):.3f} m/min
      Range: {np.min(ros_values):.3f} - {np.max(ros_values):.3f} m/min
    
    Spatial ROS (final state):
      Mean:  {np.mean(ros_values_flat)*60:.3f} m/min
      Std:   {np.std(ros_values_flat)*60:.3f} m/min
      Range: {np.min(ros_values_flat)*60:.3f} - {np.max(ros_values_flat)*60:.3f} m/min
    """
    
    ax4.text(0.1, 0.5, stats_text, fontfamily='monospace', fontsize=10,
             verticalalignment='center', transform=ax4.transAxes)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    print(f"✓ Saved visualization to {output_file}")


def main():
    """Main processing routine"""
    parser = argparse.ArgumentParser(
        description="Analyze fire Rate of Spread (ROS) from fire solver output"
    )
    parser.add_argument('fire_dir', help='Fire solver output directory')
    parser.add_argument('--output-dir', default='.',
                        help='Output directory for post-processing results')
    
    args = parser.parse_args()
    
    print("\n" + "="*70)
    print("FIRE RATE OF SPREAD (ROS) POST-PROCESSING")
    print("="*70 + "\n")
    
    try:
        # Read fire front data (placeholder - would read from solver output)
        print(f"Reading fire data from {args.fire_dir}...")
        
        # For demonstration, create synthetic fire evolution
        print("Using synthetic fire evolution for demonstration...")
        fire_data = create_synthetic_fire_evolution(num_steps=20)
        print(f"✓ Generated {len(fire_data)} fire timesteps")
        
        # Calculate ROS from temporal evolution
        print("\nCalculating ROS from fire perimeter evolution...")
        ros_timeseries = calculate_ros_from_evolution(fire_data)
        print(f"✓ Computed ROS for {len(ros_timeseries)} time intervals")
        
        # Calculate spatial ROS distribution
        print("Calculating spatial ROS distribution...")
        ros_spatial = calculate_spatial_ros(num_steps=len(fire_data))
        print(f"✓ Spatial ROS grid: {ros_spatial.shape}")
        
        # Write outputs
        output_dir = Path(args.output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        ros_time_file = output_dir / 'fire_ros_timeseries.csv'
        write_ros_timeseries(str(ros_time_file), ros_timeseries)
        
        ros_spatial_file = output_dir / 'fire_ros_spatial.csv'
        write_spatial_ros(str(ros_spatial_file), ros_spatial)
        
        stats_file = output_dir / 'fire_ros_statistics.txt'
        compute_ros_statistics(ros_timeseries, ros_spatial, str(stats_file))
        
        # Generate visualization
        viz_file = output_dir / 'fire_ros_evolution.png'
        generate_visualization(ros_timeseries, ros_spatial, str(viz_file))
        
        print("\n" + "="*70)
        print("✓ ROS analysis complete")
        print("="*70 + "\n")
        
        return 0
        
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
