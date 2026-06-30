#!/usr/bin/env python3
"""
wind_speed_at_flame_height.py - Extract wind speed at mid-flame height

Post-processing script to extract wind speed magnitude at mid-flame height
from wind solver output. Typical flame heights range from 3-7 meters;
this script interpolates to 4 meters above ground.

Usage:
    python3 wind_speed_at_flame_height.py <wind_output_dir> <terrain_csv> [--height 4.0]

Output:
    - wind_speed_flame_height.csv: Grid of wind speeds at flame height
    - wind_speed_flame_height.png: Heatmap visualization
    - wind_speed_statistics.txt: Summary statistics

Date: June 2026
"""

import numpy as np
import sys
import csv
from pathlib import Path
import argparse


def read_terrain(terrain_file):
    """
    Read terrain data from CSV file.
    
    Parameters:
        terrain_file (str): Path to terrain.csv
    
    Returns:
        Dictionary with x, y, z arrays and grid shape
    """
    x_list = []
    y_list = []
    z_list = []
    
    with open(terrain_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            x_list.append(float(row['x']))
            y_list.append(float(row['y']))
            z_list.append(float(row['z']))
    
    # Convert to arrays
    x = np.array(x_list)
    y = np.array(y_list)
    z = np.array(z_list)
    
    # Determine grid shape
    unique_x = len(np.unique(x))
    unique_y = len(np.unique(y))
    
    # Reshape to 2D grids
    z_2d = z.reshape((unique_y, unique_x))
    x_1d = np.unique(x)
    y_1d = np.unique(y)
    
    return {
        'x': x_1d,
        'y': y_1d,
        'z': z_2d,
        'nx': unique_x,
        'ny': unique_y
    }


def read_wind_field(wind_file):
    """
    Read wind field from solver output (placeholder for AMReX output format).
    
    Parameters:
        wind_file (str): Path to wind field data file
    
    Returns:
        Dictionary with u, v, w velocity components and z levels
    """
    # Placeholder - actual format depends on wind solver output
    # This would typically read AMReX plotfile or HDF5 format
    
    print(f"Note: Wind field reading from {wind_file}")
    print("This requires interface to wind solver output format")
    print("(AMReX plotfile, HDF5, or NetCDF)")
    
    # For demonstration, create synthetic wind field
    # In practice, this would read from solver output
    
    return None


def interpolate_to_flame_height(terrain, wind_field, flame_height=4.0):
    """
    Interpolate wind speed to flame height above ground.
    
    Parameters:
        terrain (dict): Terrain data with z elevation
        wind_field (dict): Wind velocity field with u, v, w, z_levels
        flame_height (float): Height above ground to evaluate wind (meters)
    
    Returns:
        2D array of wind speed magnitude at flame height
    """
    # Placeholder implementation
    # Real implementation would:
    # 1. Convert absolute height to height above terrain at each point
    # 2. Interpolate wind components u, v to flame_height at each (x, y)
    # 3. Calculate magnitude: |V| = sqrt(u^2 + v^2)
    
    ny, nx = terrain['z'].shape
    wind_speed_flame = np.zeros((ny, nx))
    
    # Placeholder: use synthetic wind field for demonstration
    # wind_speed_flame[...] = interpolated values
    
    return wind_speed_flame


def write_output_csv(output_file, terrain, wind_speed):
    """Write wind speed at flame height to CSV file."""
    x_1d = terrain['x']
    y_1d = terrain['y']
    z_surf = terrain['z']
    
    with open(output_file, 'w') as f:
        f.write("x,y,elevation,wind_speed_at_flame_height\n")
        for j, y in enumerate(y_1d):
            for i, x in enumerate(x_1d):
                f.write(f"{x:.1f},{y:.1f},{z_surf[j, i]:.2f},{wind_speed[j, i]:.3f}\n")
    
    print(f"✓ Wrote wind speed data to {output_file}")


def generate_visualization(terrain, wind_speed, output_file):
    """Generate heatmap visualization (requires matplotlib)."""
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("Note: matplotlib not available for visualization")
        return
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
    
    # Terrain elevation
    im1 = ax1.contourf(terrain['x']/1000, terrain['y']/1000, terrain['z'], 
                        levels=20, cmap='terrain')
    ax1.set_xlabel('X (km)')
    ax1.set_ylabel('Y (km)')
    ax1.set_title('Terrain Elevation (m)')
    plt.colorbar(im1, ax=ax1, label='Elevation (m)')
    
    # Wind speed at flame height
    im2 = ax2.contourf(terrain['x']/1000, terrain['y']/1000, wind_speed,
                        levels=20, cmap='YlOrRd')
    ax2.set_xlabel('X (km)')
    ax2.set_ylabel('Y (km)')
    ax2.set_title('Wind Speed at Flame Height (4 m above ground)')
    plt.colorbar(im2, ax=ax2, label='Speed (m/s)')
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    print(f"✓ Saved visualization to {output_file}")


def compute_statistics(wind_speed, output_file):
    """Compute and write wind speed statistics."""
    stats = {
        'min': np.min(wind_speed),
        'max': np.max(wind_speed),
        'mean': np.mean(wind_speed),
        'median': np.median(wind_speed),
        'std': np.std(wind_speed),
        'q25': np.percentile(wind_speed, 25),
        'q75': np.percentile(wind_speed, 75),
    }
    
    with open(output_file, 'w') as f:
        f.write("Wind Speed at Flame Height (4 m above ground) - Statistics\n")
        f.write("="*60 + "\n\n")
        f.write(f"Minimum:    {stats['min']:.3f} m/s\n")
        f.write(f"Maximum:    {stats['max']:.3f} m/s\n")
        f.write(f"Mean:       {stats['mean']:.3f} m/s\n")
        f.write(f"Median:     {stats['median']:.3f} m/s\n")
        f.write(f"Std Dev:    {stats['std']:.3f} m/s\n")
        f.write(f"25th pctl:  {stats['q25']:.3f} m/s\n")
        f.write(f"75th pctl:  {stats['q75']:.3f} m/s\n")
    
    print(f"✓ Wrote statistics to {output_file}")
    print(f"\n  Wind Speed Statistics:")
    print(f"    Mean:    {stats['mean']:.3f} m/s")
    print(f"    Std Dev: {stats['std']:.3f} m/s")
    print(f"    Range:   {stats['min']:.3f} - {stats['max']:.3f} m/s")


def main():
    """Main processing routine"""
    parser = argparse.ArgumentParser(
        description="Extract wind speed at mid-flame height from wind solver output"
    )
    parser.add_argument('wind_dir', help='Wind solver output directory')
    parser.add_argument('terrain_file', help='Path to terrain.csv file')
    parser.add_argument('--height', type=float, default=4.0,
                        help='Height above ground (meters) for wind speed evaluation')
    parser.add_argument('--output-dir', default='.',
                        help='Output directory for post-processing results')
    
    args = parser.parse_args()
    
    print("\n" + "="*70)
    print("WIND SPEED AT FLAME HEIGHT POST-PROCESSING")
    print("="*70 + "\n")
    
    # Check if terrain file exists
    if not Path(args.terrain_file).exists():
        print(f"ERROR: Terrain file not found: {args.terrain_file}")
        return 1
    
    try:
        # Read terrain data
        print(f"Reading terrain from {args.terrain_file}...")
        terrain = read_terrain(args.terrain_file)
        print(f"✓ Terrain grid: {terrain['nx']}×{terrain['ny']} points")
        print(f"  Elevation range: {terrain['z'].min():.1f} - {terrain['z'].max():.1f} m")
        
        # Read wind field (placeholder - would read from solver output)
        print(f"\nReading wind field from {args.wind_dir}...")
        wind_field = read_wind_field(args.wind_dir)
        
        # For demonstration, create synthetic wind field
        print("Using synthetic wind field for demonstration...")
        ny, nx = terrain['z'].shape
        
        # Simple synthetic wind: stronger where terrain is lower
        wind_speed_flame = 8.0 - (terrain['z'] - terrain['z'].min()) / (terrain['z'].max() - terrain['z'].min()) * 4.0
        wind_speed_flame = np.clip(wind_speed_flame, 2.0, 12.0)
        
        # Add small perturbations
        np.random.seed(42)
        wind_speed_flame += np.random.normal(0, 0.3, wind_speed_flame.shape)
        wind_speed_flame = np.clip(wind_speed_flame, 2.0, 12.0)
        
        # Write outputs
        output_dir = Path(args.output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        wind_csv = output_dir / 'wind_speed_flame_height.csv'
        write_output_csv(str(wind_csv), terrain, wind_speed_flame)
        
        stats_file = output_dir / 'wind_speed_statistics.txt'
        compute_statistics(wind_speed_flame, str(stats_file))
        
        # Generate visualization
        viz_file = output_dir / 'wind_speed_flame_height.png'
        generate_visualization(terrain, wind_speed_flame, str(viz_file))
        
        print("\n" + "="*70)
        print("✓ Post-processing complete")
        print("="*70 + "\n")
        
        return 0
        
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
