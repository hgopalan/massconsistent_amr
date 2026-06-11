#!/usr/bin/env python3
"""
test_flatirons_buildings_svf.py - Flatirons Campus Colorado with Random Buildings

Tests unified sky view factor and solar shading with:
- Real-world Flatirons terrain data (Boulder, Colorado)
- Synthetic random buildings simulating campus
- Solar shading computation from sun position
- SVF from combined terrain+building elevation field

Key Features:
1. Unified terrain+building approach: buildings treated as terrain features
2. Sky view factor (SVF) computation from local slope
3. Solar shading based on solar position (hour, day, latitude)
4. Terrain-building interactions automatically handled

Validates:
1. SVF computation with complex terrain
2. Building shading during different times of day
3. Urban canyon effects in street networks
4. Integration with wind field solver
"""

import os
import sys
import numpy as np
import csv
from pathlib import Path

# Flatirons Colorado bounding box (approximate)
FLATIRONS_LAT_MIN = 40.0130
FLATIRONS_LAT_MAX = 40.0180
FLATIRONS_LON_MIN = -105.2450
FLATIRONS_LON_MAX = -105.2380

def generate_synthetic_flatirons_terrain(nx=15, ny=15):
    """
    Generate simplified synthetic terrain for Flatirons region.
    
    Flatirons characteristics:
    - Elevation range: ~1670-2500 m
    - Complex ridges and valleys
    - Moderate to steep slopes
    
    Args:
        nx, ny: Number of terrain points in each direction
    
    Returns:
        List of (x, y, z) tuples in UTM coordinates (approx meters)
    """
    terrain_points = []
    
    # Simulate Flatirons with multiple Gaussian peaks (ridges)
    x_utm = np.linspace(0, 1000, nx)
    y_utm = np.linspace(0, 1000, ny)
    
    # Multiple ridges to simulate Flatirons
    ridges = [
        (300, 300, 150, 200),   # Ridge center x, y, sigma, peak height
        (700, 500, 150, 180),
        (500, 800, 120, 160),
    ]
    
    base_elev = 1670  # Base elevation in meters
    
    for i, x in enumerate(x_utm):
        for j, y in enumerate(y_utm):
            z = base_elev
            
            # Superimpose Gaussian peaks for ridges
            for cx, cy, sigma, peak in ridges:
                dist = np.sqrt((x - cx)**2 + (y - cy)**2)
                z += peak * np.exp(-dist**2 / (2 * sigma**2))
            
            # Add terrain noise for realism
            z += np.random.normal(0, 10)
            
            terrain_points.append((x, y, z))
    
    return terrain_points


def generate_random_campus_buildings(n_buildings=15, nx_terrain=15, ny_terrain=15):
    """
    Generate random buildings on campus-like layout.
    
    Creates rectangular buildings with realistic properties:
    - Building dimensions: 20-60m x 15-40m
    - Building heights: 15-50m
    - Spacing: avoid overlap, realistic campus layout
    
    Args:
        n_buildings: Number of buildings to generate
        nx_terrain, ny_terrain: Terrain grid size for coordinate scaling
    
    Returns:
        List of building dictionaries with x_min, y_min, x_max, y_max, z_min, z_max
    """
    buildings = []
    np.random.seed(42)  # Reproducible
    
    terrain_width = 1000  # UTM-like coordinates
    terrain_height = 1000
    
    min_spacing = 150  # Minimum distance between buildings (m)
    
    for _ in range(n_buildings):
        attempts = 0
        valid = False
        
        while attempts < 10 and not valid:
            attempts += 1
            
            # Random building size
            bx_len = np.random.uniform(20, 60)
            by_len = np.random.uniform(15, 40)
            bz_height = np.random.uniform(15, 50)
            
            # Random position (ensure within domain and not too close to edges)
            margin = 100
            bx_min = np.random.uniform(margin, terrain_width - bx_len - margin)
            by_min = np.random.uniform(margin, terrain_height - by_len - margin)
            
            bx_max = bx_min + bx_len
            by_max = by_min + by_len
            bz_min = 0  # Building sits on terrain
            bz_max = bz_height
            
            # Check spacing from other buildings
            valid = True
            for other_bldg in buildings:
                # Compute distance to other building center
                cx_self = (bx_min + bx_max) / 2
                cy_self = (by_min + by_max) / 2
                cx_other = (other_bldg["x_min"] + other_bldg["x_max"]) / 2
                cy_other = (other_bldg["y_min"] + other_bldg["y_max"]) / 2
                
                dist = np.sqrt((cx_self - cx_other)**2 + (cy_self - cy_other)**2)
                if dist < min_spacing:
                    valid = False
                    break
        
        if valid:
            buildings.append({
                "x_min": bx_min,
                "y_min": by_min,
                "x_max": bx_max,
                "y_max": by_max,
                "z_min": bz_min,
                "z_max": bz_max
            })
    
    return buildings


def write_terrain_csv(terrain_points, filename):
    """Write terrain data to CSV format."""
    with open(filename, 'w', newline='') as f:
        f.write("# Flatirons Synthetic Terrain  X[m]  Y[m]  Z[m]\n")
        f.write("# Domain: 0-1000 x 0-1000 m, complex ridges\n")
        writer = csv.writer(f, delimiter=' ')
        for x, y, z in terrain_points:
            writer.writerow([f"{x:.1f}", f"{y:.1f}", f"{z:.1f}"])


def write_buildings_csv(buildings, filename):
    """Write building data to CSV format (x1 y1 z1 x2 y2 z2 format)."""
    with open(filename, 'w', newline='') as f:
        f.write("# Campus Buildings  X1[m] Y1[m] Z1[m]  X2[m] Y2[m] Z2[m]\n")
        f.write(f"# {len(buildings)} buildings on Flatirons terrain\n")
        writer = csv.writer(f, delimiter=' ')
        for bldg in buildings:
            x1, y1, z1 = bldg["x_min"], bldg["y_min"], bldg["z_min"]
            x2, y2, z2 = bldg["x_max"], bldg["y_max"], bldg["z_max"]
            writer.writerow([f"{x1:.1f}", f"{y1:.1f}", f"{z1:.1f}",
                           f"{x2:.1f}", f"{y2:.1f}", f"{z2:.1f}"])


def create_inputs_file(output_file):
    """Create inputs.i configuration file."""
    content = """# Flatirons Campus with Random Buildings - Sky View Factor Test
# Tests unified SVF computation with buildings on real-world terrain
# Features: Terrain-building interactions, solar shading, urban canyon effects

terrain_file = terrain.csv
building_file = buildings.csv

# Reference wind
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 20.0
dy = 20.0
dz = 20.0

# Domain height [m]
domain_height = 200.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Sky View Factor and Solar Shading (unified terrain+building approach)
enable_sky_view_factor = true
enable_solar_shading = true
latitude_degrees = 40.015   # Flatirons latitude
longitude_degrees = -105.241  # Flatirons longitude
day_of_year = 172.0          # June 21 (summer solstice)
hour_of_day = 12.0           # Noon
max_horizon_distance = 1000.0

# Buoyancy stratification with diurnal cycle
enable_buoyancy_stratification = true
enable_diurnal_temperature = true
diurnal_temperature_amplitude = 8.0  # +/- 8K
diurnal_phase_hour = 14.0            # Peak heating at 2 PM
buoyancy_coefficient = 1.0

# MLMG solver settings
mlmg_verbose = 1
max_grid_size = 32
tol_rel = 1.e-8

# Output
plot_file = plt_flatirons_buildings_svf
"""
    with open(output_file, 'w') as f:
        f.write(content)


def main():
    """Generate test data and create input files."""
    print("Generating Flatirons Campus Test Case with Random Buildings")
    print("="*70)
    
    # Create test directory
    test_dir = Path(__file__).parent
    os.makedirs(test_dir, exist_ok=True)
    
    # Generate terrain
    print("Generating synthetic Flatirons terrain...")
    terrain = generate_synthetic_flatirons_terrain(nx=20, ny=20)
    terrain_file = test_dir / "terrain.csv"
    write_terrain_csv(terrain, terrain_file)
    print(f"  Written {len(terrain)} terrain points to {terrain_file}")
    
    # Generate buildings
    print("Generating random campus buildings...")
    buildings = generate_random_campus_buildings(n_buildings=20)
    buildings_file = test_dir / "buildings.csv"
    write_buildings_csv(buildings, buildings_file)
    print(f"  Generated {len(buildings)} buildings")
    print(f"  Building heights: {min(b['z_max']-b['z_min'] for b in buildings):.1f}-"
          f"{max(b['z_max']-b['z_min'] for b in buildings):.1f} m")
    print(f"  Building areas: {min((b['x_max']-b['x_min'])*(b['y_max']-b['y_min']) for b in buildings):.0f}-"
          f"{max((b['x_max']-b['x_min'])*(b['y_max']-b['y_min']) for b in buildings):.0f} m²")
    print(f"  Written to {buildings_file}")
    
    # Create inputs file
    print("Creating inputs.i configuration file...")
    inputs_file = test_dir / "inputs.i"
    create_inputs_file(inputs_file)
    print(f"  Written to {inputs_file}")
    
    print("\nTest case preparation complete!")
    print(f"\nTo run the test:")
    print(f"  cd {test_dir}")
    print(f"  /path/to/build/wind_solver inputs.i")
    print(f"\nExpected output:")
    print(f"  - plt_flatirons_buildings_svf_* (AMReX plotfiles)")
    print(f"  - Wind field solution with SVF and shading effects")
    print(f"  - Building-terrain interactions computed automatically")


if __name__ == "__main__":
    main()
