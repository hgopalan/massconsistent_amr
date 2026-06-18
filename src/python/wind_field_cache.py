#!/usr/bin/env python3
"""
wind_field_cache.py - Wind field caching and serialization utilities

Provides efficient caching of solved wind fields from the mass-consistent
wind solver, enabling rapid evaluation of multiple wind farm layouts without
re-solving the wind field.

Features:
- HDF5-based persistence for large 3D wind fields
- Efficient memory caching with lazy loading
- Terrain elevation and domain metadata storage
- Fast retrieval and interpolation-ready format

Example:
    from wind_solver import WindSolver
    from wind_field_cache import WindFieldCache
    
    # Solve wind field
    wind = WindSolver("inputs.i")
    wind.solve()
    
    # Cache the results
    cache = WindFieldCache.from_solver(wind)
    cache.save("wind_field_cache.h5")
    
    # Later, load cached field for rapid evaluation
    cached = WindFieldCache.load("wind_field_cache.h5")
    u = cached.interpolate_u(x=100.0, y=200.0, z=90.0)
"""

import numpy as np
import h5py
import pickle
from pathlib import Path
from typing import Dict, Tuple, Optional, Any
import json


class WindFieldCache:
    """
    Efficient caching container for mass-consistent wind field solutions.
    
    Stores 3D velocity components, terrain elevation, and domain metadata
    in a format optimized for rapid layout evaluation and interpolation.
    
    Attributes:
        u_field (ndarray): U-velocity component (nz, ny, nx)
        v_field (ndarray): V-velocity component (nz, ny, nx)
        w_field (ndarray): W-velocity component (nz, ny, nx)
        terrain (ndarray): Terrain elevation (ny, nx)
        grid_x (ndarray): X-coordinates of grid points
        grid_y (ndarray): Y-coordinates of grid points
        grid_z (ndarray): Z-coordinates of grid points
        nx, ny, nz (int): Grid dimensions
        dx, dy, dz (float): Grid spacing
        xmin, ymin, zmin (float): Domain lower bounds
        metadata (dict): Additional solver metadata
    """
    
    def __init__(self):
        """Initialize empty wind field cache."""
        self.u_field: Optional[np.ndarray] = None
        self.v_field: Optional[np.ndarray] = None
        self.w_field: Optional[np.ndarray] = None
        self.terrain: Optional[np.ndarray] = None
        
        self.grid_x: Optional[np.ndarray] = None
        self.grid_y: Optional[np.ndarray] = None
        self.grid_z: Optional[np.ndarray] = None
        
        self.nx: int = 0
        self.ny: int = 0
        self.nz: int = 0
        
        self.dx: float = 0.0
        self.dy: float = 0.0
        self.dz: float = 0.0
        
        self.xmin: float = 0.0
        self.ymin: float = 0.0
        self.zmin: float = 0.0
        
        self.metadata: Dict[str, Any] = {}
    
    @classmethod
    def from_solver(cls, wind_solver) -> 'WindFieldCache':
        """
        Create cache from a solved WindSolver instance.
        
        Parameters:
            wind_solver: Initialized and solved WindSolver instance
        
        Returns:
            WindFieldCache: Populated with solver data
        
        Raises:
            RuntimeError: If solver is not solved
        """
        if not wind_solver.solved:
            raise RuntimeError("Wind solver must be solved before caching")
        
        cache = cls()
        
        # Extract velocity field
        vel = wind_solver.get_velocity()
        cache.u_field = vel['u'].copy()
        cache.v_field = vel['v'].copy()
        cache.w_field = vel['w'].copy()
        
        # Extract terrain
        cache.terrain = wind_solver.get_terrain().copy()
        
        # Extract grid information
        cache.nx = wind_solver.nx
        cache.ny = wind_solver.ny
        cache.nz = wind_solver.nz
        cache.dx = wind_solver.dx
        cache.dy = wind_solver.dy
        cache.dz = wind_solver.dz
        cache.xmin = wind_solver.xmin
        cache.ymin = wind_solver.ymin
        cache.zmin = wind_solver.zmin
        
        # Create grid coordinates
        cache.grid_x = np.linspace(
            cache.xmin, 
            cache.xmin + (cache.nx - 1) * cache.dx, 
            cache.nx
        )
        cache.grid_y = np.linspace(
            cache.ymin, 
            cache.ymin + (cache.ny - 1) * cache.dy, 
            cache.ny
        )
        cache.grid_z = np.linspace(
            cache.zmin, 
            cache.zmin + (cache.nz - 1) * cache.dz, 
            cache.nz
        )
        
        # Store metadata
        cache.metadata = {
            'solver_residual': wind_solver.residual,
            'solver_iterations': wind_solver.iters,
            'terrain_min_m': float(cache.terrain.min()),
            'terrain_max_m': float(cache.terrain.max()),
        }
        
        print(f"✓ Wind field cached: {cache.nx}×{cache.ny}×{cache.nz} grid")
        return cache
    
    def save(self, filename: str) -> None:
        """
        Save cached wind field to HDF5 file.
        
        Parameters:
            filename (str): Output HDF5 filename
        
        Returns:
            None (writes to file)
        """
        with h5py.File(filename, 'w') as f:
            # Store velocity fields
            f.create_dataset('u_field', data=self.u_field, compression='gzip')
            f.create_dataset('v_field', data=self.v_field, compression='gzip')
            f.create_dataset('w_field', data=self.w_field, compression='gzip')
            f.create_dataset('terrain', data=self.terrain, compression='gzip')
            
            # Store grid coordinates
            f.create_dataset('grid_x', data=self.grid_x)
            f.create_dataset('grid_y', data=self.grid_y)
            f.create_dataset('grid_z', data=self.grid_z)
            
            # Store scalar grid information
            f.attrs['nx'] = self.nx
            f.attrs['ny'] = self.ny
            f.attrs['nz'] = self.nz
            f.attrs['dx'] = self.dx
            f.attrs['dy'] = self.dy
            f.attrs['dz'] = self.dz
            f.attrs['xmin'] = self.xmin
            f.attrs['ymin'] = self.ymin
            f.attrs['zmin'] = self.zmin
            
            # Store metadata as JSON string
            f.attrs['metadata_json'] = json.dumps(self.metadata)
        
        print(f"✓ Wind field cache saved to {filename}")
    
    @classmethod
    def load(cls, filename: str) -> 'WindFieldCache':
        """
        Load cached wind field from HDF5 file.
        
        Parameters:
            filename (str): Input HDF5 filename
        
        Returns:
            WindFieldCache: Loaded cache object
        
        Raises:
            FileNotFoundError: If file does not exist
            KeyError: If HDF5 structure is invalid
        """
        if not Path(filename).exists():
            raise FileNotFoundError(f"Cache file not found: {filename}")
        
        cache = cls()
        
        with h5py.File(filename, 'r') as f:
            # Load velocity fields
            cache.u_field = f['u_field'][:]
            cache.v_field = f['v_field'][:]
            cache.w_field = f['w_field'][:]
            cache.terrain = f['terrain'][:]
            
            # Load grid coordinates
            cache.grid_x = f['grid_x'][:]
            cache.grid_y = f['grid_y'][:]
            cache.grid_z = f['grid_z'][:]
            
            # Load scalar attributes
            cache.nx = int(f.attrs['nx'])
            cache.ny = int(f.attrs['ny'])
            cache.nz = int(f.attrs['nz'])
            cache.dx = float(f.attrs['dx'])
            cache.dy = float(f.attrs['dy'])
            cache.dz = float(f.attrs['dz'])
            cache.xmin = float(f.attrs['xmin'])
            cache.ymin = float(f.attrs['ymin'])
            cache.zmin = float(f.attrs['zmin'])
            
            # Load metadata
            if 'metadata_json' in f.attrs:
                cache.metadata = json.loads(f.attrs['metadata_json'])
        
        print(f"✓ Wind field cache loaded from {filename}")
        return cache
    
    def get_domain_bounds(self) -> Dict[str, float]:
        """
        Get domain boundary coordinates.
        
        Returns:
            dict: {xmin, xmax, ymin, ymax, zmin, zmax} in meters
        """
        return {
            'xmin': self.xmin,
            'xmax': self.xmin + (self.nx - 1) * self.dx,
            'ymin': self.ymin,
            'ymax': self.ymin + (self.ny - 1) * self.dy,
            'zmin': self.zmin,
            'zmax': self.zmin + (self.nz - 1) * self.dz,
        }
    
    def is_point_in_domain(self, x: float, y: float, z: float) -> bool:
        """
        Check if a point is within the cached wind field domain.
        
        Parameters:
            x, y, z (float): Point coordinates in meters
        
        Returns:
            bool: True if point is in domain, False otherwise
        """
        bounds = self.get_domain_bounds()
        return (
            bounds['xmin'] <= x <= bounds['xmax'] and
            bounds['ymin'] <= y <= bounds['ymax'] and
            bounds['zmin'] <= z <= bounds['zmax']
        )
    
    def interpolate_velocity_trilinear(
        self, 
        x: float, 
        y: float, 
        z: float
    ) -> Tuple[float, float, float]:
        """
        Perform trilinear interpolation of velocity at a 3D point.
        
        Parameters:
            x, y, z (float): Physical coordinates in meters
        
        Returns:
            (u, v, w) tuple of interpolated velocity components
        
        Raises:
            ValueError: If point is outside domain
        """
        bounds = self.get_domain_bounds()
        
        # Check bounds
        if not self.is_point_in_domain(x, y, z):
            raise ValueError(
                f"Point ({x}, {y}, {z}) outside domain: "
                f"x∈[{bounds['xmin']}, {bounds['xmax']}], "
                f"y∈[{bounds['ymin']}, {bounds['ymax']}], "
                f"z∈[{bounds['zmin']}, {bounds['zmax']}]"
            )
        
        # Find grid indices
        i_x = (x - self.xmin) / self.dx
        i_y = (y - self.ymin) / self.dy
        i_z = (z - self.zmin) / self.dz
        
        # Get lower indices (floor)
        i0_x = int(np.floor(i_x))
        i0_y = int(np.floor(i_y))
        i0_z = int(np.floor(i_z))
        
        # Get upper indices (ceil, clamped)
        i1_x = min(i0_x + 1, self.nx - 1)
        i1_y = min(i0_y + 1, self.ny - 1)
        i1_z = min(i0_z + 1, self.nz - 1)
        
        # Get fractional parts
        fx = i_x - i0_x
        fy = i_y - i0_y
        fz = i_z - i0_z
        
        # Trilinear interpolation for u, v, w
        def _trilinear_interp(field):
            """Helper to perform trilinear interpolation on a field."""
            f000 = field[i0_z, i0_y, i0_x]
            f100 = field[i0_z, i0_y, i1_x]
            f010 = field[i0_z, i1_y, i0_x]
            f110 = field[i0_z, i1_y, i1_x]
            f001 = field[i1_z, i0_y, i0_x]
            f101 = field[i1_z, i0_y, i1_x]
            f011 = field[i1_z, i1_y, i0_x]
            f111 = field[i1_z, i1_y, i1_x]
            
            return (
                f000 * (1 - fx) * (1 - fy) * (1 - fz) +
                f100 * fx * (1 - fy) * (1 - fz) +
                f010 * (1 - fx) * fy * (1 - fz) +
                f110 * fx * fy * (1 - fz) +
                f001 * (1 - fx) * (1 - fy) * fz +
                f101 * fx * (1 - fy) * fz +
                f011 * (1 - fx) * fy * fz +
                f111 * fx * fy * fz
            )
        
        u_interp = _trilinear_interp(self.u_field)
        v_interp = _trilinear_interp(self.v_field)
        w_interp = _trilinear_interp(self.w_field)
        
        return float(u_interp), float(v_interp), float(w_interp)
    
    def interpolate_u(self, x: float, y: float, z: float) -> float:
        """Get interpolated u-velocity at point."""
        u, _, _ = self.interpolate_velocity_trilinear(x, y, z)
        return u
    
    def interpolate_v(self, x: float, y: float, z: float) -> float:
        """Get interpolated v-velocity at point."""
        _, v, _ = self.interpolate_velocity_trilinear(x, y, z)
        return v
    
    def interpolate_w(self, x: float, y: float, z: float) -> float:
        """Get interpolated w-velocity at point."""
        _, _, w = self.interpolate_velocity_trilinear(x, y, z)
        return w
    
    def get_wind_speed_and_direction(
        self, 
        x: float, 
        y: float, 
        z: float
    ) -> Tuple[float, float]:
        """
        Get wind speed and direction at a point.
        
        Parameters:
            x, y, z (float): Point coordinates in meters
        
        Returns:
            (speed_ms, direction_deg) tuple where:
                speed_ms: Wind speed magnitude (m/s)
                direction_deg: Wind direction from north, 0-360 degrees
        """
        u, v, _ = self.interpolate_velocity_trilinear(x, y, z)
        
        speed = np.sqrt(u**2 + v**2)
        # Meteorological convention: direction is where wind comes FROM
        direction = np.degrees(np.arctan2(u, v)) % 360.0
        
        return float(speed), float(direction)
    
    def get_terrain_elevation(self, x: float, y: float) -> float:
        """
        Get terrain elevation at (x, y) using bilinear interpolation.
        
        Parameters:
            x, y (float): Horizontal coordinates in meters
        
        Returns:
            float: Terrain elevation in meters
        """
        # Find grid indices with clamping
        i_x = (x - self.xmin) / self.dx
        i_y = (y - self.ymin) / self.dy
        
        i0_x = int(np.clip(np.floor(i_x), 0, self.nx - 2))
        i0_y = int(np.clip(np.floor(i_y), 0, self.ny - 2))
        i1_x = i0_x + 1
        i1_y = i0_y + 1
        
        fx = i_x - i0_x
        fy = i_y - i0_y
        
        # Bilinear interpolation
        z_interp = (
            self.terrain[i0_y, i0_x] * (1 - fx) * (1 - fy) +
            self.terrain[i0_y, i1_x] * fx * (1 - fy) +
            self.terrain[i1_y, i0_x] * (1 - fx) * fy +
            self.terrain[i1_y, i1_x] * fx * fy
        )
        
        return float(z_interp)
    
    def __repr__(self) -> str:
        """String representation."""
        bounds = self.get_domain_bounds()
        return (
            f"WindFieldCache(grid={self.nx}×{self.ny}×{self.nz}, "
            f"domain=[{bounds['xmin']:.0f}, {bounds['xmax']:.0f}]×"
            f"[{bounds['ymin']:.0f}, {bounds['ymax']:.0f}]×"
            f"[{bounds['zmin']:.0f}, {bounds['zmax']:.0f}])"
        )
