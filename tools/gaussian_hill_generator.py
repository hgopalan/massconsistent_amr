#!/usr/bin/env python3
"""
gaussian_hill_generator.py - Generate Gaussian hill terrain profiles.

Provides utilities for creating synthetic Gaussian hill terrain for testing
the mass-consistent wind solver with various hill sizes and dimensions.

Example:
    # Generate default Gaussian hill
    generator = GaussianHillGenerator()
    terrain = generator.generate()
    generator.write_terrain_csv("terrain.csv")
    
    # Generate custom Gaussian hill
    custom_hill = GaussianHillGenerator(
        nx=21, ny=21,
        domain_x=500.0, domain_y=500.0,
        peak_height=100.0,
        sigma=75.0
    )
    custom_hill.write_terrain_csv("custom_terrain.csv")
"""

import math
import sys
from pathlib import Path


class GaussianHillGenerator:
    """
    Generate synthetic Gaussian hill terrain profiles.
    
    A Gaussian hill is defined by:
    z(x, y) = z_peak * exp(-((x - xc)^2 + (y - yc)^2) / (2 * sigma^2))
    
    Attributes:
        nx, ny (int): Number of grid points in x and y directions
        xmin, xmax, ymin, ymax (float): Domain bounds in meters
        z_peak (float): Maximum elevation at domain center in meters
        sigma (float): Gaussian width parameter in meters
        terrain (list): List of (x, y, z) tuples for all grid points
    """
    
    def __init__(self, nx=11, ny=11, domain_x=300.0, domain_y=300.0,
                 peak_height=50.0, sigma=60.0, xmin=0.0, ymin=0.0):
        """
        Initialize Gaussian hill generator.
        
        Parameters:
            nx (int): Number of grid points in x-direction (default: 11)
            ny (int): Number of grid points in y-direction (default: 11)
            domain_x (float): Domain extent in x-direction in meters (default: 300)
            domain_y (float): Domain extent in y-direction in meters (default: 300)
            peak_height (float): Peak elevation at center in meters (default: 50)
            sigma (float): Gaussian width (std dev) in meters (default: 60)
            xmin (float): X-coordinate of domain minimum (default: 0)
            ymin (float): Y-coordinate of domain minimum (default: 0)
        """
        self.nx = nx
        self.ny = ny
        self.xmin = xmin
        self.xmax = xmin + domain_x
        self.ymin = ymin
        self.ymax = ymin + domain_y
        self.z_peak = peak_height
        self.sigma = sigma
        
        # Center of domain
        self.xc = (self.xmin + self.xmax) / 2.0
        self.yc = (self.ymin + self.ymax) / 2.0
        
        self.terrain = []
    
    def generate(self):
        """
        Generate Gaussian hill terrain.
        
        Returns:
            list: List of (x, y, z) tuples for all grid points
        """
        self.terrain = []
        
        for j in range(self.ny):
            y = self.ymin + j * (self.ymax - self.ymin) / (self.ny - 1)
            for i in range(self.nx):
                x = self.xmin + i * (self.xmax - self.xmin) / (self.nx - 1)
                
                # Gaussian hill: z = z_peak * exp(-r^2 / (2 * sigma^2))
                r_squared = (x - self.xc)**2 + (y - self.yc)**2
                z = self.z_peak * math.exp(-r_squared / (2.0 * self.sigma**2))
                
                self.terrain.append((x, y, z))
        
        return self.terrain
    
    def write_terrain_csv(self, filename):
        """
        Write terrain to CSV file in format: X Y Z
        
        Parameters:
            filename (str): Output CSV filename
        
        Returns:
            bool: True on success
        """
        if not self.terrain:
            self.generate()
        
        try:
            with open(filename, 'w') as f:
                # Write header comment
                f.write(f"# Gaussian hill terrain  X[m]  Y[m]  Z[m]\n")
                f.write(f"# Domain: {self.xmin}-{self.xmax} x {self.ymin}-{self.ymax} m, "
                       f"peak={self.z_peak} m at ({self.xc},{self.yc}), sigma={self.sigma} m\n")
                
                # Write terrain points
                for x, y, z in self.terrain:
                    f.write(f"{x:.6f} {y:.6f} {z:.6f}\n")
            
            return True
        
        except Exception as e:
            print(f"ERROR: Failed to write terrain CSV: {e}", file=sys.stderr)
            return False
    
    def get_stats(self):
        """
        Get statistics about the generated terrain.
        
        Returns:
            dict: Dictionary with terrain statistics
        """
        if not self.terrain:
            self.generate()
        
        z_values = [z for _, _, z in self.terrain]
        z_min = min(z_values) if z_values else 0
        z_max = max(z_values) if z_values else 0
        z_mean = sum(z_values) / len(z_values) if z_values else 0
        
        return {
            'grid_points': len(self.terrain),
            'grid_nx': self.nx,
            'grid_ny': self.ny,
            'domain_x': self.xmax - self.xmin,
            'domain_y': self.ymax - self.ymin,
            'dx': (self.xmax - self.xmin) / (self.nx - 1) if self.nx > 1 else 0,
            'dy': (self.ymax - self.ymin) / (self.ny - 1) if self.ny > 1 else 0,
            'peak_height': self.z_peak,
            'sigma': self.sigma,
            'z_min': z_min,
            'z_max': z_max,
            'z_mean': z_mean,
        }
    
    def print_stats(self):
        """Print terrain statistics to stderr."""
        stats = self.get_stats()
        print(f"Generated Gaussian hill terrain:", file=sys.stderr)
        print(f"  Grid: {stats['grid_nx']}x{stats['grid_ny']} points", file=sys.stderr)
        print(f"  Domain: {stats['domain_x']:.1f}m x {stats['domain_y']:.1f}m", file=sys.stderr)
        print(f"  Grid spacing: dx={stats['dx']:.2f}m, dy={stats['dy']:.2f}m", file=sys.stderr)
        print(f"  Peak height: {stats['peak_height']:.1f}m at ({self.xc:.1f}, {self.yc:.1f})", 
              file=sys.stderr)
        print(f"  Sigma (width): {stats['sigma']:.1f}m", file=sys.stderr)
        print(f"  Elevation range: {stats['z_min']:.2f}m to {stats['z_max']:.2f}m", file=sys.stderr)


def main():
    """Command-line interface for Gaussian hill generator."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Generate Gaussian hill terrain for mass-consistent wind solver"
    )
    parser.add_argument('--output', '-o', default='terrain.csv',
                       help='Output CSV filename (default: terrain.csv)')
    parser.add_argument('--nx', type=int, default=11,
                       help='Number of grid points in x-direction (default: 11)')
    parser.add_argument('--ny', type=int, default=11,
                       help='Number of grid points in y-direction (default: 11)')
    parser.add_argument('--domain-x', type=float, default=300.0,
                       help='Domain extent in x-direction [m] (default: 300)')
    parser.add_argument('--domain-y', type=float, default=300.0,
                       help='Domain extent in y-direction [m] (default: 300)')
    parser.add_argument('--peak', type=float, default=50.0,
                       help='Peak elevation [m] (default: 50)')
    parser.add_argument('--sigma', type=float, default=60.0,
                       help='Gaussian width parameter [m] (default: 60)')
    parser.add_argument('--xmin', type=float, default=0.0,
                       help='X-coordinate of domain minimum [m] (default: 0)')
    parser.add_argument('--ymin', type=float, default=0.0,
                       help='Y-coordinate of domain minimum [m] (default: 0)')
    
    args = parser.parse_args()
    
    # Create generator
    gen = GaussianHillGenerator(
        nx=args.nx,
        ny=args.ny,
        domain_x=args.domain_x,
        domain_y=args.domain_y,
        peak_height=args.peak,
        sigma=args.sigma,
        xmin=args.xmin,
        ymin=args.ymin
    )
    
    # Generate terrain
    gen.generate()
    gen.print_stats()
    
    # Write to file
    if gen.write_terrain_csv(args.output):
        print(f"✓ Terrain written to {args.output}", file=sys.stderr)
        return 0
    else:
        print(f"✗ Failed to write terrain to {args.output}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
