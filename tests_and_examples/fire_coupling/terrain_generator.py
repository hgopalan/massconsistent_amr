#!/usr/bin/env python3
"""
terrain_generator.py - Generate synthetic terrain for fire coupling examples

Creates SRTM-like terrain CSV files for Colorado and California fire scenarios.
Uses Perlin noise to generate realistic topography matching typical elevations.

Date: June 2026
"""

import numpy as np
import sys
from pathlib import Path

def generate_perlin_noise(shape, scale=10, octaves=4, persistence=0.5, lacunarity=2.0, seed=42):
    """
    Generate Perlin-like noise using simple fractal Brownian motion.
    
    Parameters:
        shape: (ny, nx) tuple for grid size
        scale: Initial noise scale
        octaves: Number of noise octaves
        persistence: Amplitude decay per octave
        lacunarity: Frequency increase per octave
        seed: Random seed
    
    Returns:
        2D numpy array with noise values normalized to [0, 1]
    """
    np.random.seed(seed)
    ny, nx = shape
    noise = np.zeros((ny, nx))
    
    amplitude = 1.0
    frequency = 1.0
    max_value = 0.0
    
    for _ in range(octaves):
        # Create random phase shifts
        phase_x = np.random.rand() * 2 * np.pi
        phase_y = np.random.rand() * 2 * np.pi
        
        # Create grid of coordinates
        x = np.arange(nx) / scale * frequency
        y = np.arange(ny) / scale * frequency
        X, Y = np.meshgrid(x, y)
        
        # Simple sine-based noise (approximates Perlin)
        octave = amplitude * (
            np.sin(X + phase_x) * 0.5 +
            np.sin(Y + phase_y) * 0.5
        ) * 0.5
        
        noise += octave
        max_value += amplitude
        
        amplitude *= persistence
        frequency *= lacunarity
    
    # Normalize to [0, 1]
    noise = (noise / max_value + 1.0) / 2.0
    return np.clip(noise, 0, 1)


def generate_colorado_terrain(nx=156, ny=156, dx=64.0, dy=64.0):
    """
    Generate Colorado high-elevation terrain (mountainous).
    
    Returns:
        Dictionary with x, y coordinates and elevation z
    """
    x = np.arange(nx) * dx
    y = np.arange(ny) * dy
    X, Y = np.meshgrid(x, y)
    
    # Generate base terrain with Perlin noise
    noise = generate_perlin_noise((ny, nx), scale=30, octaves=5, seed=2024)
    
    # Add ridge features (higher on west/northwest side)
    ridge_strength = 200.0
    ridge = ridge_strength * np.exp(-((X - 5000)**2 + (Y - 3000)**2) / (1000**2))
    
    # Add valley feature (lower on east side)
    valley = -100.0 * np.exp(-((X - 8000)**2) / (2000**2))
    
    # Base elevation: 2000m with variations
    base_elevation = 2000.0
    z = base_elevation + noise * 400.0 + ridge + valley
    
    # Ensure realistic range (1800-2800 m for Colorado high terrain)
    z = np.clip(z, 1800.0, 2800.0)
    
    return {'x': x, 'y': y, 'z': z, 'X': X, 'Y': Y}


def generate_california_terrain(nx=156, ny=156, dx=64.0, dy=64.0):
    """
    Generate California coastal terrain (moderate elevation with ridges).
    
    Returns:
        Dictionary with x, y coordinates and elevation z
    """
    x = np.arange(nx) * dx
    y = np.arange(ny) * dy
    X, Y = np.meshgrid(x, y)
    
    # Generate base terrain with Perlin noise
    noise = generate_perlin_noise((ny, nx), scale=25, octaves=4, seed=2025)
    
    # Add coastal ridge (north-south oriented, higher on west)
    ridge_strength = 150.0
    ridge = ridge_strength * np.exp(-((X - 3000)**2) / (1500**2))
    
    # Add coastal plain (lower on east)
    plain = -80.0 * np.exp(-((X - 8000)**2) / (2500**2))
    
    # Base elevation: 400m with variations
    base_elevation = 400.0
    z = base_elevation + noise * 250.0 + ridge + plain
    
    # Ensure realistic range (200-1200 m for northern California coastal)
    z = np.clip(z, 200.0, 1200.0)
    
    return {'x': x, 'y': y, 'z': z, 'X': X, 'Y': Y}


def write_terrain_csv(filepath, terrain_data):
    """
    Write terrain data to CSV file in format expected by wind solver.
    
    Format: x, y, z (one point per line)
    """
    x = terrain_data['x']
    y = terrain_data['y']
    z = terrain_data['z']
    X = terrain_data['X']
    Y = terrain_data['Y']
    
    with open(filepath, 'w') as f:
        f.write("x,y,z\n")
        for j in range(z.shape[0]):
            for i in range(z.shape[1]):
                f.write(f"{X[j, i]:.1f},{Y[j, i]:.1f},{z[j, i]:.2f}\n")
    
    print(f"✓ Generated {filepath}")
    print(f"  Points: {z.shape[0]} × {z.shape[1]}")
    print(f"  Elevation range: {z.min():.1f} - {z.max():.1f} m")


def main():
    """Generate terrain files for fire coupling examples"""
    
    base_dir = Path(__file__).resolve().parent
    nx, ny = 156, 156  # 10 km / 64 m ≈ 156 cells
    dx, dy = 64.0, 64.0
    
    print("\n" + "="*70)
    print("GENERATING SYNTHETIC TERRAIN FOR FIRE COUPLING EXAMPLES")
    print("="*70 + "\n")
    
    # Colorado terrain
    print("Generating Colorado terrain (high-elevation mountains)...")
    colorado = generate_colorado_terrain(nx, ny, dx, dy)
    
    # Write to all Colorado scenario subdirectories
    for scenario in ['wind_only', 'fire_one_way', 'fire_two_way']:
        output_file = base_dir / f"colorado/{scenario}/terrain.csv"
        write_terrain_csv(str(output_file), colorado)
    
    print()
    
    # California terrain
    print("Generating California terrain (coastal mountains)...")
    california = generate_california_terrain(nx, ny, dx, dy)
    
    # Write to all California scenario subdirectories
    for scenario in ['wind_only', 'fire_one_way', 'fire_two_way']:
        output_file = base_dir / f"california/{scenario}/terrain.csv"
        write_terrain_csv(str(output_file), california)
    
    print("\n" + "="*70)
    print("✓ Terrain generation complete")
    print("="*70 + "\n")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
