#!/usr/bin/env python3
"""
Receptor Grid Generator
=======================

Generate receptor grids for concentration output sampling in atmospheric dispersion models.
Supports regular grids, irregular patterns, impact assessment zones, and custom coordinates.

Features:
- Regular 2D/3D receptor grids
- Radial pattern from source
- Along-wind and cross-wind transects
- Irregular custom patterns
- Zone-based sampling (impact zones)
- CSV export compatible with puff model
- Visualization of receptor placement

Usage:
    python receptor_grid_generator.py --grid 2d --nx 20 --ny 20 --x0 -1000 --y0 -1000 --dx 100 --output receptors.csv
    python receptor_grid_generator.py --pattern radial --nradii 5 --ntheta 12 --rmax 5000 --output receptors_radial.csv
    python receptor_grid_generator.py --zones impact --downwind-dist 10000 --lateral-dist 5000
"""

import sys
import argparse
import csv
import math
from pathlib import Path
from typing import List, Tuple, Optional
from dataclasses import dataclass
from enum import Enum


class GridType(Enum):
    """Grid pattern types."""
    GRID_2D = "2d"              # Regular 2D rectangular grid
    GRID_3D = "3d"              # Regular 3D grid
    RADIAL = "radial"           # Concentric radial pattern
    TRANSECT = "transect"       # Along-wind and cross-wind lines
    ZONES = "zones"             # Hazard zone boundaries
    CUSTOM = "custom"           # Custom from file


@dataclass
class Receptor:
    """Receptor location definition."""
    x: float                    # [m]
    y: float                    # [m]
    z: float                    # [m]
    label: str                  # Name/ID
    
    def to_csv_row(self) -> List[str]:
        """Convert to CSV row format."""
        return [f"{self.x:.1f}", f"{self.y:.1f}", f"{self.z:.1f}", self.label]
    
    @classmethod
    def from_csv_row(cls, row: List[str]) -> "Receptor":
        """Create from CSV row."""
        return cls(
            x=float(row[0]),
            y=float(row[1]),
            z=float(row[2]),
            label=row[3] if len(row) > 3 else f"R_{row[0]}_{row[1]}_{row[2]}"
        )


def generate_2d_grid(
    nx: int,
    ny: int,
    x0: float = -1000.0,
    y0: float = -1000.0,
    dx: float = 100.0,
    dy: float = 100.0,
    z: float = 1.5
) -> List[Receptor]:
    """
    Generate regular 2D rectangular grid of receptors.
    
    Parameters
    ----------
    nx : int
        Number of grid points in x-direction
    ny : int
        Number of grid points in y-direction
    x0 : float
        Origin x-coordinate [m]
    y0 : float
        Origin y-coordinate [m]
    dx : float
        Grid spacing in x [m]
    dy : float
        Grid spacing in y [m]
    z : float
        Receptor height [m]
    
    Returns
    -------
    List[Receptor]
        Receptor locations
    """
    receptors = []
    
    for i in range(nx):
        for j in range(ny):
            x = x0 + i * dx
            y = y0 + j * dy
            label = f"R_2D_{i:03d}_{j:03d}"
            receptors.append(Receptor(x=x, y=y, z=z, label=label))
    
    return receptors


def generate_3d_grid(
    nx: int,
    ny: int,
    nz: int,
    x0: float = -1000.0,
    y0: float = -1000.0,
    z0: float = 1.5,
    dx: float = 100.0,
    dy: float = 100.0,
    dz: float = 25.0
) -> List[Receptor]:
    """
    Generate regular 3D grid of receptors.
    
    Parameters
    ----------
    nx, ny, nz : int
        Grid dimensions
    x0, y0, z0 : float
        Origin coordinates [m]
    dx, dy, dz : float
        Grid spacing [m]
    
    Returns
    -------
    List[Receptor]
        Receptor locations
    """
    receptors = []
    
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                x = x0 + i * dx
                y = y0 + j * dy
                z = z0 + k * dz
                label = f"R_3D_{i:02d}_{j:02d}_{k:02d}"
                receptors.append(Receptor(x=x, y=y, z=z, label=label))
    
    return receptors


def generate_radial_grid(
    nradii: int,
    ntheta: int,
    rmax: float = 5000.0,
    x_center: float = 0.0,
    y_center: float = 0.0,
    z: float = 1.5,
    theta_offset: float = 0.0
) -> List[Receptor]:
    """
    Generate radial grid pattern (concentric circles).
    
    Parameters
    ----------
    nradii : int
        Number of radial points
    ntheta : int
        Number of azimuthal points per radius
    rmax : float
        Maximum radius [m]
    x_center : float
        Grid center x-coordinate [m]
    y_center : float
        Grid center y-coordinate [m]
    z : float
        Receptor height [m]
    theta_offset : float
        Starting angle offset [degrees]
    
    Returns
    -------
    List[Receptor]
        Receptor locations
    """
    receptors = []
    theta_offset_rad = math.radians(theta_offset)
    
    for i in range(nradii):
        r = (i + 1) / nradii * rmax  # Linear radius spacing
        
        for j in range(ntheta):
            theta = 2 * math.pi * j / ntheta + theta_offset_rad
            
            x = x_center + r * math.cos(theta)
            y = y_center + r * math.sin(theta)
            label = f"R_RAD_{i:02d}_{j:03d}"
            
            receptors.append(Receptor(x=x, y=y, z=z, label=label))
    
    return receptors


def generate_transect_grid(
    source_x: float,
    source_y: float,
    wind_direction: float = 270.0,
    downwind_distances: Optional[List[float]] = None,
    crosswind_distances: Optional[List[float]] = None,
    z: float = 1.5
) -> List[Receptor]:
    """
    Generate along-wind and cross-wind transect receptors.
    
    Parameters
    ----------
    source_x, source_y : float
        Source location [m]
    wind_direction : float
        Wind direction [degrees, 0=N, 90=E, 180=S, 270=W]
    downwind_distances : List[float]
        Downwind distances to sample [m]
    crosswind_distances : List[float]
        Cross-wind offsets to sample [m]
    z : float
        Receptor height [m]
    
    Returns
    -------
    List[Receptor]
        Receptor locations
    """
    if downwind_distances is None:
        downwind_distances = [500, 1000, 2000, 5000, 10000]
    
    if crosswind_distances is None:
        crosswind_distances = [-1000, -500, 0, 500, 1000]
    
    receptors = []
    wind_rad = math.radians(wind_direction)
    
    for i, d_down in enumerate(downwind_distances):
        for j, d_cross in enumerate(crosswind_distances):
            # Downwind direction
            x_down = source_x + d_down * math.cos(wind_rad)
            y_down = source_y + d_down * math.sin(wind_rad)
            
            # Cross-wind offset (perpendicular to wind)
            cross_rad = wind_rad + math.pi / 2
            x = x_down + d_cross * math.cos(cross_rad)
            y = y_down + d_cross * math.sin(cross_rad)
            
            label = f"R_TRANS_{i:02d}_{j:02d}"
            receptors.append(Receptor(x=x, y=y, z=z, label=label))
    
    return receptors


def generate_zone_grid(
    source_x: float,
    source_y: float,
    wind_direction: float = 270.0,
    downwind_extent: float = 20000.0,
    lateral_extent: float = 10000.0,
    red_distance: float = 1000.0,
    orange_distance: float = 5000.0,
    yellow_distance: float = 10000.0,
    npoints_per_zone: int = 20
) -> List[Receptor]:
    """
    Generate receptors along hazard zone boundaries.
    
    Parameters
    ----------
    source_x, source_y : float
        Source location [m]
    wind_direction : float
        Predominant wind direction [degrees]
    downwind_extent : float
        Downwind extent of zones [m]
    lateral_extent : float
        Lateral extent of zones [m]
    red_distance : float
        Red zone radius [m]
    orange_distance : float
        Orange zone radius [m]
    yellow_distance : float
        Yellow zone radius [m]
    npoints_per_zone : int
        Receptors per zone boundary
    
    Returns
    -------
    List[Receptor]
        Receptor locations along zone boundaries
    """
    receptors = []
    wind_rad = math.radians(wind_direction)
    
    zones = [
        ("red", red_distance),
        ("orange", orange_distance),
        ("yellow", yellow_distance)
    ]
    
    for zone_name, zone_radius in zones:
        # Create arc downwind of source
        for i in range(npoints_per_zone):
            # Arc from -60° to +60° relative to wind direction
            angle = (i - npoints_per_zone / 2) / npoints_per_zone * math.pi / 3
            angle_rad = wind_rad + angle
            
            x = source_x + zone_radius * math.cos(angle_rad)
            y = source_y + zone_radius * math.sin(angle_rad)
            
            label = f"R_{zone_name.upper()}_{i:02d}"
            receptors.append(Receptor(x=x, y=y, z=1.5, label=label))
    
    return receptors


def write_receptor_csv(
    receptors: List[Receptor],
    output_file: str,
    description: str = "Receptor grid"
) -> None:
    """
    Write receptor grid to CSV file.
    
    Parameters
    ----------
    receptors : List[Receptor]
        Receptor locations
    output_file : str
        Output CSV file path
    description : str
        Optional description
    """
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Write metadata header
        writer.writerow(['# Receptor Grid'])
        writer.writerow(['# Description:', description])
        writer.writerow(['# Generated by receptor_grid_generator.py'])
        writer.writerow([])
        
        # Write column headers
        writer.writerow(['x [m]', 'y [m]', 'z [m]', 'label'])
        
        # Write receptors
        for receptor in receptors:
            writer.writerow(receptor.to_csv_row())
    
    print(f"✓ Wrote receptor grid to {output_file}")
    print(f"  {len(receptors)} receptors")
    
    # Print statistics
    xs = [r.x for r in receptors]
    ys = [r.y for r in receptors]
    zs = [r.z for r in receptors]
    
    print(f"  X range: [{min(xs):.1f}, {max(xs):.1f}] m")
    print(f"  Y range: [{min(ys):.1f}, {max(ys):.1f}] m")
    print(f"  Z range: [{min(zs):.1f}, {max(zs):.1f}] m")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Generate receptor grids for concentration sampling",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Regular 2D grid covering 2000x2000 m with 100 m spacing at 1.5 m height
  python receptor_grid_generator.py --grid 2d --nx 20 --ny 20 \\
    --x0 -1000 --y0 -1000 --dx 100 --dy 100 --output grid_2d.csv
  
  # Radial pattern with 5 radii, 12 points per radius, max radius 5 km
  python receptor_grid_generator.py --pattern radial \\
    --nradii 5 --ntheta 12 --rmax 5000 --output grid_radial.csv
  
  # Along and cross-wind transects from stack at (0,0)
  python receptor_grid_generator.py --pattern transect \\
    --source-x 0 --source-y 0 --wind-direction 270 --output transects.csv
  
  # Hazard impact zones
  python receptor_grid_generator.py --zones impact \\
    --source-x 0 --source-y 0 --wind-direction 270 \\
    --red 1000 --orange 5000 --yellow 10000 --output zones.csv
        """
    )
    
    # Grid/Pattern selection
    pattern_group = parser.add_mutually_exclusive_group(required=True)
    pattern_group.add_argument(
        "--grid",
        choices=["2d", "3d"],
        help="Generate regular grid pattern"
    )
    pattern_group.add_argument(
        "--pattern",
        choices=["radial", "transect"],
        help="Generate spatial pattern"
    )
    pattern_group.add_argument(
        "--zones",
        choices=["impact"],
        help="Generate hazard zone boundaries"
    )
    
    # Common parameters
    parser.add_argument(
        "--output",
        required=True,
        help="Output CSV filename"
    )
    
    parser.add_argument(
        "--z",
        type=float,
        default=1.5,
        help="Receptor height [m] (default: 1.5)"
    )
    
    # 2D/3D grid parameters
    parser.add_argument("--nx", type=int, default=20, help="Grid points in X")
    parser.add_argument("--ny", type=int, default=20, help="Grid points in Y")
    parser.add_argument("--nz", type=int, default=3, help="Grid points in Z")
    parser.add_argument("--x0", type=float, default=-1000.0, help="Origin X [m]")
    parser.add_argument("--y0", type=float, default=-1000.0, help="Origin Y [m]")
    parser.add_argument("--z0", type=float, default=1.5, help="Origin Z [m]")
    parser.add_argument("--dx", type=float, default=100.0, help="Spacing X [m]")
    parser.add_argument("--dy", type=float, default=100.0, help="Spacing Y [m]")
    parser.add_argument("--dz", type=float, default=25.0, help="Spacing Z [m]")
    
    # Radial parameters
    parser.add_argument("--nradii", type=int, default=5, help="Number of radii")
    parser.add_argument("--ntheta", type=int, default=12, help="Points per radius")
    parser.add_argument("--rmax", type=float, default=5000.0, help="Max radius [m]")
    
    # Center parameters
    parser.add_argument("--x-center", type=float, default=0.0, help="Grid center X [m]")
    parser.add_argument("--y-center", type=float, default=0.0, help="Grid center Y [m]")
    
    # Source and transect parameters
    parser.add_argument("--source-x", type=float, default=0.0, help="Source X [m]")
    parser.add_argument("--source-y", type=float, default=0.0, help="Source Y [m]")
    parser.add_argument("--wind-direction", type=float, default=270.0, help="Wind direction [degrees]")
    
    # Zone parameters
    parser.add_argument("--red", type=float, default=1000.0, help="Red zone radius [m]")
    parser.add_argument("--orange", type=float, default=5000.0, help="Orange zone radius [m]")
    parser.add_argument("--yellow", type=float, default=10000.0, help="Yellow zone radius [m]")
    parser.add_argument("--npoints", type=int, default=20, help="Points per zone")
    
    args = parser.parse_args()
    
    # Generate appropriate receptor grid
    if args.grid == "2d":
        receptors = generate_2d_grid(args.nx, args.ny, args.x0, args.y0, args.dx, args.dy, args.z)
    elif args.grid == "3d":
        receptors = generate_3d_grid(args.nx, args.ny, args.nz, args.x0, args.y0, args.z0, args.dx, args.dy, args.dz)
    elif args.pattern == "radial":
        receptors = generate_radial_grid(args.nradii, args.ntheta, args.rmax, args.x_center, args.y_center, args.z)
    elif args.pattern == "transect":
        receptors = generate_transect_grid(args.source_x, args.source_y, args.wind_direction, z=args.z)
    elif args.zones == "impact":
        receptors = generate_zone_grid(
            args.source_x, args.source_y, args.wind_direction,
            red_distance=args.red, orange_distance=args.orange, yellow_distance=args.yellow,
            npoints_per_zone=args.npoints
        )
    else:
        print("Error: No valid pattern selected")
        sys.exit(1)
    
    # Determine description
    desc = "Receptor grid"
    if args.grid:
        desc = f"{args.grid.upper()} grid: {args.nx}×{args.ny}" + (f"×{args.nz}" if args.grid == "3d" else "")
    elif args.pattern:
        desc = f"{args.pattern.upper()} pattern"
    elif args.zones:
        desc = f"Impact zone boundaries"
    
    write_receptor_csv(receptors, args.output, description=desc)


if __name__ == "__main__":
    main()
