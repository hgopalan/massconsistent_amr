#!/usr/bin/env python3
"""
terrain_reader_srtm.py - Read and process SRTM terrain data.

Integrated from wildfire_levelset project.
Provides utilities for reading SRTM DEM (Digital Elevation Model) data
and converting to formats compatible with the mass-consistent wind solver.

SRTM data characteristics:
- 1-arcsecond resolution: ~30m at equator
- 3-arcsecond resolution: ~90m at equator  
- Organized in 1x1 degree tiles
- Available from USGS SRTM data servers

Example:
    # Read SRTM HGT file
    reader = SRTMReader("N40W105.hgt")  # Boulder/Flatirons area
    dem = reader.read()
    
    # Sample grid points
    points = dem.sample_points(nx=21, ny=21)
    dem.write_terrain_csv("terrain.csv", points)
"""

import struct
import os
import sys
from pathlib import Path
from typing import Tuple, List, Optional
import numpy as np


class SRTMTile:
    """
    Represents a single SRTM tile (1x1 degree).
    
    SRTM data format:
    - Binary .hgt file containing elevation data
    - Organized as matrix of int16 values
    - No-Data value: -32768
    """
    
    TILE_SIZE = 3601  # Standard SRTM tile size (3601x3601 for 1-second resolution)
    NO_DATA_VALUE = -32768
    
    def __init__(self, filename: str):
        """
        Initialize SRTM tile from .hgt file.
        
        Parameters:
            filename (str): Path to SRTM .hgt file (e.g., "N40W105.hgt")
        """
        self.filename = filename
        self.data = None
        self.lat_min = None
        self.lon_min = None
        self._parse_coordinates()
    
    def _parse_coordinates(self):
        """
        Extract coordinates from filename.
        
        Standard SRTM naming: NSSDDWWWDDD.hgt
        - N/S: North/South latitude
        - SS: latitude degrees (00-90)
        - W/E: West/East longitude
        - WWWDDD: longitude degrees (000-180)
        
        Example: N40W105.hgt = 40°N, 105°W
        """
        basename = os.path.basename(self.filename).upper()
        
        if not basename.endswith('.HGT'):
            raise ValueError(f"Expected .hgt file, got {basename}")
        
        name = basename[:-4]  # Remove extension
        
        if len(name) != 7:
            raise ValueError(f"Invalid SRTM filename format: {basename}")
        
        # Parse latitude
        lat_sign = 1 if name[0] == 'N' else -1
        lat_deg = int(name[1:3])
        lat0 = lat_sign * lat_deg
        lat1 = lat_sign * (lat_deg - 1) if name[0] == 'S' else lat_sign * (lat_deg + 1)
        self.lat_min = min(lat0, lat1)
        self.lat_max = max(lat0, lat1)
        
        # Parse longitude
        lon_sign = -1 if name[3] == 'W' else 1
        lon_deg = int(name[4:7])
        lon0 = lon_sign * lon_deg
        lon1 = lon_sign * (lon_deg - 1) if name[3] == 'W' else lon_sign * (lon_deg + 1)
        self.lon_min = min(lon0, lon1)
        self.lon_max = max(lon0, lon1)
    
    def read(self) -> bool:
        """
        Read SRTM data from .hgt file.
        
        Returns:
            bool: True if successful
        """
        if not os.path.exists(self.filename):
            print(f"ERROR: File not found: {self.filename}", file=sys.stderr)
            return False
        
        try:
            expected_size = self.TILE_SIZE * self.TILE_SIZE * 2  # 2 bytes per int16
            file_size = os.path.getsize(self.filename)
            
            if file_size != expected_size:
                print(f"WARNING: File size {file_size} doesn't match expected {expected_size}",
                      file=sys.stderr)
            
            # Read binary data
            with open(self.filename, 'rb') as f:
                data_bytes = f.read(file_size)
            
            # Unpack as big-endian int16 (SRTM standard)
            num_values = len(data_bytes) // 2
            self.data = struct.unpack(f'>{num_values}h', data_bytes)
            self.data = np.array(self.data, dtype=np.float32)
            
            # Reshape to 2D grid
            self.data = self.data.reshape((self.TILE_SIZE, self.TILE_SIZE))
            
            # Replace no-data values with NaN
            self.data[self.data == self.NO_DATA_VALUE] = np.nan
            
            print(f"✓ Loaded SRTM tile: {os.path.basename(self.filename)}", file=sys.stderr)
            print(f"  Coordinates: {self.lat_min}° to {self.lat_max}°N, "
                  f"{self.lon_min}° to {self.lon_max}°E", file=sys.stderr)
            print(f"  Elevation range: {np.nanmin(self.data):.1f}m to {np.nanmax(self.data):.1f}m",
                  file=sys.stderr)
            
            return True
        
        except Exception as e:
            print(f"ERROR: Failed to read SRTM file: {e}", file=sys.stderr)
            return False
    
    def get_elevation(self, lat: float, lon: float) -> Optional[float]:
        """
        Get elevation at a specific latitude/longitude.
        
        Uses bilinear interpolation for sub-grid accuracy.
        
        Parameters:
            lat (float): Latitude in degrees
            lon (float): Longitude in degrees
        
        Returns:
            float: Elevation in meters, or None if out of bounds or no data
        """
        if self.data is None:
            return None
        
        if not (self.lat_min <= lat <= self.lat_max and 
                self.lon_min <= lon <= self.lon_max):
            return None
        
        # Convert lat/lon to array indices
        row = (self.lat_max - lat) / (self.lat_max - self.lat_min) * (self.TILE_SIZE - 1)
        col = (lon - self.lon_min) / (self.lon_max - self.lon_min) * (self.TILE_SIZE - 1)
        
        # Clamp to valid range
        row = np.clip(row, 0, self.TILE_SIZE - 1)
        col = np.clip(col, 0, self.TILE_SIZE - 1)
        
        # Bilinear interpolation
        r0, c0 = int(row), int(col)
        r1, c1 = min(r0 + 1, self.TILE_SIZE - 1), min(c0 + 1, self.TILE_SIZE - 1)
        
        dr, dc = row - r0, col - c0
        
        z00 = self.data[r0, c0]
        z01 = self.data[r0, c1]
        z10 = self.data[r1, c0]
        z11 = self.data[r1, c1]
        
        # Check for no-data values
        if np.isnan(z00) or np.isnan(z01) or np.isnan(z10) or np.isnan(z11):
            return None
        
        # Bilinear interpolation formula
        z = (1 - dr) * (1 - dc) * z00 + \
            (1 - dr) * dc * z01 + \
            dr * (1 - dc) * z10 + \
            dr * dc * z11
        
        return float(z)


class SRTMReader:
    """Read and process SRTM terrain data."""
    
    def __init__(self, *tile_files):
        """
        Initialize SRTM reader.
        
        Parameters:
            *tile_files: One or more SRTM .hgt filenames
        """
        self.tiles = []
        self.tile_dict = {}
        
        for filename in tile_files:
            tile = SRTMTile(filename)
            self.tiles.append(tile)
            key = (tile.lat_min, tile.lon_min)
            self.tile_dict[key] = tile
    
    def read(self) -> bool:
        """
        Read all SRTM tiles.
        
        Returns:
            bool: True if all tiles read successfully
        """
        success = True
        for tile in self.tiles:
            if not tile.read():
                success = False
        return success
    
    def get_elevation(self, lat: float, lon: float) -> Optional[float]:
        """
        Get elevation at a specific latitude/longitude.
        
        Automatically selects the correct tile.
        
        Parameters:
            lat (float): Latitude in degrees
            lon (float): Longitude in degrees
        
        Returns:
            float: Elevation in meters, or None if not available
        """
        for tile in self.tiles:
            elev = tile.get_elevation(lat, lon)
            if elev is not None:
                return elev
        return None
    
    def sample_points(self, lat_min: float, lat_max: float, 
                     lon_min: float, lon_max: float,
                     nx: int = 21, ny: int = 21) -> List[Tuple[float, float, float]]:
        """
        Sample elevation at grid points.
        
        Parameters:
            lat_min, lat_max (float): Latitude range in degrees
            lon_min, lon_max (float): Longitude range in degrees
            nx, ny (int): Number of grid points (default: 21x21)
        
        Returns:
            list: List of (lat, lon, elevation) tuples
        """
        points = []
        
        for j in range(ny):
            lat = lat_min + j * (lat_max - lat_min) / (ny - 1)
            for i in range(nx):
                lon = lon_min + i * (lon_max - lon_min) / (nx - 1)
                
                elev = self.get_elevation(lat, lon)
                if elev is None:
                    elev = 0.0  # Default to sea level if no data
                
                points.append((lat, lon, elev))
        
        return points
    
    def write_terrain_csv(self, output_file: str, 
                         points: Optional[List[Tuple[float, float, float]]] = None,
                         lat_min: float = None, lat_max: float = None,
                         lon_min: float = None, lon_max: float = None,
                         nx: int = 21, ny: int = 21) -> bool:
        """
        Write terrain to CSV file compatible with wind solver.
        
        Parameters:
            output_file (str): Output CSV filename
            points (list, optional): List of (lat, lon, elev) tuples
                                    If None, samples from lat/lon bounds
            lat_min, lat_max (float): Latitude bounds for sampling
            lon_min, lon_max (float): Longitude bounds for sampling
            nx, ny (int): Grid dimensions for sampling (default: 21x21)
        
        Returns:
            bool: True on success
        """
        try:
            if points is None:
                if lat_min is None or lat_max is None or lon_min is None or lon_max is None:
                    print("ERROR: Must provide either points or lat/lon bounds", 
                          file=sys.stderr)
                    return False
                points = self.sample_points(lat_min, lat_max, lon_min, lon_max, nx, ny)
            
            # Convert lat/lon to UTM or projected coordinates
            # For simplicity, convert degrees to approximate meters
            # 1 degree latitude ≈ 111 km
            # 1 degree longitude ≈ 111 km * cos(latitude)
            
            with open(output_file, 'w') as f:
                # Write header
                f.write("# SRTM terrain data (lat/lon converted to approximate meters)\n")
                f.write(f"# Grid: {nx}x{ny} points\n")
                
                # Find bounds
                lats = [p[0] for p in points]
                lons = [p[1] for p in points]
                elev = [p[2] for p in points]
                
                lat_min_data, lat_max_data = min(lats), max(lats)
                lon_min_data, lon_max_data = min(lons), max(lons)
                elev_min, elev_max = min(elev), max(elev)
                
                f.write(f"# Latitude range: {lat_min_data:.4f}° to {lat_max_data:.4f}°\n")
                f.write(f"# Longitude range: {lon_min_data:.4f}° to {lon_max_data:.4f}°\n")
                f.write(f"# Elevation range: {elev_min:.1f}m to {elev_max:.1f}m\n")
                f.write("# X[m] Y[m] Z[m]\n")
                
                # Reference point for conversion (center of domain)
                lat_ref = (lat_min_data + lat_max_data) / 2.0
                lon_ref = (lon_min_data + lon_max_data) / 2.0
                
                # Write points with coordinate conversion
                for i, (lat, lon, z) in enumerate(points):
                    # Simple conversion: 1 degree ≈ 111 km
                    x = (lon - lon_ref) * 111000.0 * np.cos(np.radians(lat_ref))
                    y = (lat - lat_ref) * 111000.0
                    
                    f.write(f"{x:.6f} {y:.6f} {z:.6f}\n")
            
            print(f"✓ Terrain written to {output_file}", file=sys.stderr)
            return True
        
        except Exception as e:
            print(f"ERROR: Failed to write terrain CSV: {e}", file=sys.stderr)
            return False


def main():
    """Command-line interface for SRTM terrain reading."""
    import argparse
    
    parser = argparse.ArgumentParser(description="Read SRTM terrain data and export to CSV")
    parser.add_argument('hgt_files', nargs='+', help='SRTM .hgt files to read')
    parser.add_argument('--output', '-o', required=True, help='Output CSV filename')
    parser.add_argument('--lat-min', type=float, required=True, help='Minimum latitude')
    parser.add_argument('--lat-max', type=float, required=True, help='Maximum latitude')
    parser.add_argument('--lon-min', type=float, required=True, help='Minimum longitude')
    parser.add_argument('--lon-max', type=float, required=True, help='Maximum longitude')
    parser.add_argument('--nx', type=int, default=21, help='Number of grid points X (default: 21)')
    parser.add_argument('--ny', type=int, default=21, help='Number of grid points Y (default: 21)')
    
    args = parser.parse_args()
    
    # Create reader
    reader = SRTMReader(*args.hgt_files)
    
    # Read tiles
    if not reader.read():
        return 1
    
    # Write terrain
    if reader.write_terrain_csv(
        args.output,
        lat_min=args.lat_min,
        lat_max=args.lat_max,
        lon_min=args.lon_min,
        lon_max=args.lon_max,
        nx=args.nx,
        ny=args.ny
    ):
        print(f"✓ Terrain exported to {args.output}")
        return 0
    else:
        return 1


if __name__ == "__main__":
    sys.exit(main())
