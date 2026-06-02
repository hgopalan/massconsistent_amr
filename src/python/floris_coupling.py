#!/usr/bin/env python3
"""
floris_coupling.py - Standalone module for exporting wind speeds to FLORIS

This module provides utilities to extract wind fields from massconsistent_amr
and export them in formats compatible with FLORIS wind farm simulation software.

It operates completely independently - no FLORIS installation is required.
Users can then use the exported data with FLORIS as needed.

Features:
    - Extract 2D wind speed maps at any height (AGL or absolute)
    - Interpolate wind to arbitrary turbine locations
    - Export to CSV, JSON, or Python dicts
    - Compute speed-up ratios relative to reference wind
    - Handle terrain-aligned wind extraction

Example:
    from wind_solver import WindSolver
    from floris_coupling import FLORISWindMap
    
    # Solve wind field
    wind = WindSolver("inputs.i")
    wind.solve()
    
    # Create wind map for FLORIS
    wind_map = FLORISWindMap(wind)
    
    # Export wind at turbine locations
    turbine_locs = [(100, 200), (300, 400), (500, 400)]
    wind_map.export_turbine_winds(
        turbine_locations=turbine_locs,
        hub_height=90.0,
        output_file="wind_data.csv"
    )
    
    # Clean up
    wind.finalize()
"""

import numpy as np
import csv
import json
from typing import Dict, List, Tuple, Optional


class FLORISWindMap:
    """
    Wind field container for FLORIS export.
    
    Handles extraction, interpolation, and export of wind data from
    massconsistent_amr for use with FLORIS or other wind farm simulators.
    
    Attributes:
        wind_solver: Reference to initialized WindSolver instance
        u_field (ndarray): 3D u-velocity component (nz, ny, nx)
        v_field (ndarray): 3D v-velocity component (nz, ny, nx)
        terrain (ndarray): 2D terrain elevation (ny, nx)
        grid_x (ndarray): 1D x-coordinates of grid points
        grid_y (ndarray): 1D y-coordinates of grid points
        grid_z (ndarray): 1D z-coordinates of grid points
        nx, ny, nz (int): Grid dimensions
        dx, dy, dz (float): Grid spacing
    """
    
    def __init__(self, wind_solver):
        """
        Initialize FLORISWindMap from a solved WindSolver instance.
        
        Parameters:
            wind_solver: Initialized and solved WindSolver instance
        
        Raises:
            RuntimeError: If solver is not initialized or solved
        """
        if not wind_solver.initialized:
            raise RuntimeError("Wind solver not initialized")
        if not wind_solver.solved:
            raise RuntimeError("Wind field not solved. Call wind.solve() first.")
        
        self.wind_solver = wind_solver
        
        # Extract wind field
        vel = wind_solver.get_velocity()
        self.u_field = vel['u']  # shape: (nz, ny, nx)
        self.v_field = vel['v']
        
        # Extract terrain
        self.terrain = wind_solver.get_terrain()  # shape: (ny, nx)
        
        # Grid info
        self.nx = wind_solver.nx
        self.ny = wind_solver.ny
        self.nz = wind_solver.nz
        self.dx = wind_solver.dx
        self.dy = wind_solver.dy
        self.dz = wind_solver.dz
        
        # Create grid coordinates
        self.xmin = wind_solver.xmin
        self.ymin = wind_solver.ymin
        self.zmin = wind_solver.zmin
        
        self.grid_x = np.linspace(self.xmin, self.xmin + (self.nx - 1) * self.dx, self.nx)
        self.grid_y = np.linspace(self.ymin, self.ymin + (self.ny - 1) * self.dy, self.ny)
        self.grid_z = np.linspace(self.zmin, self.zmin + (self.nz - 1) * self.dz, self.nz)
    
    def _interpolate_to_point(self, x: float, y: float, z: float) -> Tuple[float, float]:
        """
        Tri-linear interpolation of velocity to a 3D point.
        
        Parameters:
            x, y, z (float): Physical coordinates in meters
        
        Returns:
            (u, v) tuple of interpolated velocities at the point
        
        Raises:
            ValueError: If point is outside domain
        """
        # Check bounds
        if not (self.xmin <= x <= self.xmin + (self.nx - 1) * self.dx):
            raise ValueError(f"X coordinate {x} outside domain [{self.xmin}, {self.xmin + (self.nx - 1) * self.dx}]")
        if not (self.ymin <= y <= self.ymin + (self.ny - 1) * self.dy):
            raise ValueError(f"Y coordinate {y} outside domain [{self.ymin}, {self.ymin + (self.ny - 1) * self.dy}]")
        if not (self.zmin <= z <= self.zmin + (self.nz - 1) * self.dz):
            raise ValueError(f"Z coordinate {z} outside domain [{self.zmin}, {self.zmin + (self.nz - 1) * self.dz}]")
        
        # Find grid indices
        i_x = (x - self.xmin) / self.dx
        i_y = (y - self.ymin) / self.dy
        i_z = (z - self.zmin) / self.dz
        
        # Lower indices (floor)
        i0_x = int(np.floor(i_x))
        i0_y = int(np.floor(i_y))
        i0_z = int(np.floor(i_z))
        
        # Upper indices (ceil, clamped)
        i1_x = min(i0_x + 1, self.nx - 1)
        i1_y = min(i0_y + 1, self.ny - 1)
        i1_z = min(i0_z + 1, self.nz - 1)
        
        # Fractional parts
        fx = i_x - i0_x
        fy = i_y - i0_y
        fz = i_z - i0_z
        
        # Tri-linear interpolation for u
        u000 = self.u_field[i0_z, i0_y, i0_x]
        u100 = self.u_field[i0_z, i0_y, i1_x]
        u010 = self.u_field[i0_z, i1_y, i0_x]
        u110 = self.u_field[i0_z, i1_y, i1_x]
        u001 = self.u_field[i1_z, i0_y, i0_x]
        u101 = self.u_field[i1_z, i0_y, i1_x]
        u011 = self.u_field[i1_z, i1_y, i0_x]
        u111 = self.u_field[i1_z, i1_y, i1_x]
        
        u_interp = (
            u000 * (1 - fx) * (1 - fy) * (1 - fz) +
            u100 * fx * (1 - fy) * (1 - fz) +
            u010 * (1 - fx) * fy * (1 - fz) +
            u110 * fx * fy * (1 - fz) +
            u001 * (1 - fx) * (1 - fy) * fz +
            u101 * fx * (1 - fy) * fz +
            u011 * (1 - fx) * fy * fz +
            u111 * fx * fy * fz
        )
        
        # Tri-linear interpolation for v
        v000 = self.v_field[i0_z, i0_y, i0_x]
        v100 = self.v_field[i0_z, i0_y, i1_x]
        v010 = self.v_field[i0_z, i1_y, i0_x]
        v110 = self.v_field[i0_z, i1_y, i1_x]
        v001 = self.v_field[i1_z, i0_y, i0_x]
        v101 = self.v_field[i1_z, i0_y, i1_x]
        v011 = self.v_field[i1_z, i1_y, i0_x]
        v111 = self.v_field[i1_z, i1_y, i1_x]
        
        v_interp = (
            v000 * (1 - fx) * (1 - fy) * (1 - fz) +
            v100 * fx * (1 - fy) * (1 - fz) +
            v010 * (1 - fx) * fy * (1 - fz) +
            v110 * fx * fy * (1 - fz) +
            v001 * (1 - fx) * (1 - fy) * fz +
            v101 * fx * (1 - fy) * fz +
            v011 * (1 - fx) * fy * fz +
            v111 * fx * fy * fz
        )
        
        return u_interp, v_interp
    
    def get_wind_at_point(self, x: float, y: float, z: float) -> Dict[str, float]:
        """
        Get wind speed and direction at a 3D point.
        
        Parameters:
            x, y, z (float): Physical coordinates in meters
        
        Returns:
            dict: {
                'u': u-component (m/s),
                'v': v-component (m/s),
                'speed': Wind speed (m/s),
                'direction': Wind direction (degrees from north, 0-360),
                'x': x coordinate,
                'y': y coordinate,
                'z': z coordinate
            }
        """
        u, v = self._interpolate_to_point(x, y, z)
        
        speed = np.sqrt(u**2 + v**2)
        # Wind direction: 0°=North, 90°=East (meteorological convention)
        # atan2(v, u) gives direction wind is coming FROM
        direction = np.degrees(np.arctan2(u, v)) % 360.0
        
        return {
            'u': float(u),
            'v': float(v),
            'speed': float(speed),
            'direction': float(direction),
            'x': float(x),
            'y': float(y),
            'z': float(z)
        }
    
    def get_wind_at_turbine(self, turbine_x: float, turbine_y: float, 
                           hub_height: float) -> Dict[str, float]:
        """
        Get wind at a turbine hub location.
        
        Parameters:
            turbine_x, turbine_y (float): Turbine position in meters
            hub_height (float): Hub height above ground level (AGL) in meters
        
        Returns:
            dict: Wind data at hub (see get_wind_at_point)
        
        Raises:
            ValueError: If location is outside domain
        """
        # Get terrain elevation at turbine location (bilinear interpolation)
        i_x = (turbine_x - self.xmin) / self.dx
        i_y = (turbine_y - self.ymin) / self.dy
        
        i0_x = int(np.floor(i_x))
        i0_y = int(np.floor(i_y))
        i1_x = min(i0_x + 1, self.nx - 1)
        i1_y = min(i0_y + 1, self.ny - 1)
        
        fx = i_x - i0_x
        fy = i_y - i0_y
        
        z_terrain = (
            self.terrain[i0_y, i0_x] * (1 - fx) * (1 - fy) +
            self.terrain[i0_y, i1_x] * fx * (1 - fy) +
            self.terrain[i1_y, i0_x] * (1 - fx) * fy +
            self.terrain[i1_y, i1_x] * fx * fy
        )
        
        # Absolute height = terrain + AGL
        z_abs = z_terrain + hub_height
        
        return self.get_wind_at_point(turbine_x, turbine_y, z_abs)
    
    def get_wind_at_turbines(self, turbine_locations: List[Tuple[float, float]], 
                            hub_height: float) -> List[Dict[str, float]]:
        """
        Get wind at multiple turbine locations.
        
        Parameters:
            turbine_locations: List of (x, y) tuples in meters
            hub_height (float): Hub height AGL in meters
        
        Returns:
            List of wind data dicts (one per turbine)
        """
        winds = []
        for x, y in turbine_locations:
            winds.append(self.get_wind_at_turbine(x, y, hub_height))
        return winds
    
    def export_to_csv(self, turbine_locations: List[Tuple[float, float]], 
                     hub_height: float,
                     output_file: str,
                     reference_speed: Optional[float] = None) -> None:
        """
        Export wind data at turbine locations to CSV.
        
        Parameters:
            turbine_locations: List of (x, y) tuples
            hub_height (float): Hub height AGL in meters
            output_file (str): Output CSV filename
            reference_speed (float, optional): If provided, compute and save speed-up ratio
        
        Returns:
            None (writes to file)
        
        Example output CSV:
            turbine_id,x,y,z_terrain,z_hub,u_ms,v_ms,speed_ms,direction_deg[,speedup_ratio]
            0,100.0,200.0,50.0,140.0,5.2,1.3,5.33,345.2[,1.05]
        """
        winds = self.get_wind_at_turbines(turbine_locations, hub_height)
        
        with open(output_file, 'w', newline='') as f:
            fieldnames = ['turbine_id', 'x', 'y', 'z_terrain', 'z_hub', 
                         'u_ms', 'v_ms', 'speed_ms', 'direction_deg']
            if reference_speed is not None:
                fieldnames.append('speedup_ratio')
            
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            
            for i, wind in enumerate(winds):
                row = {
                    'turbine_id': i,
                    'x': f"{wind['x']:.2f}",
                    'y': f"{wind['y']:.2f}",
                    'z_terrain': f"{wind['z'] - hub_height:.2f}",
                    'z_hub': f"{wind['z']:.2f}",
                    'u_ms': f"{wind['u']:.3f}",
                    'v_ms': f"{wind['v']:.3f}",
                    'speed_ms': f"{wind['speed']:.3f}",
                    'direction_deg': f"{wind['direction']:.1f}"
                }
                if reference_speed is not None:
                    ratio = wind['speed'] / reference_speed if reference_speed > 0 else 1.0
                    row['speedup_ratio'] = f"{ratio:.4f}"
                
                writer.writerow(row)
        
        print(f"✓ Exported wind data for {len(winds)} turbines to {output_file}")
    
    def export_to_json(self, turbine_locations: List[Tuple[float, float]], 
                      hub_height: float,
                      output_file: str,
                      reference_speed: Optional[float] = None) -> None:
        """
        Export wind data at turbine locations to JSON.
        
        Parameters:
            turbine_locations: List of (x, y) tuples
            hub_height (float): Hub height AGL in meters
            output_file (str): Output JSON filename
            reference_speed (float, optional): If provided, compute and save speed-up ratio
        
        Returns:
            None (writes to file)
        """
        winds = self.get_wind_at_turbines(turbine_locations, hub_height)
        
        data = {
            'solver_info': {
                'nx': self.nx,
                'ny': self.ny,
                'nz': self.nz,
                'dx_m': self.dx,
                'dy_m': self.dy,
                'dz_m': self.dz,
                'domain_x_range': [self.xmin, self.xmin + (self.nx - 1) * self.dx],
                'domain_y_range': [self.ymin, self.ymin + (self.ny - 1) * self.dy],
                'domain_z_range': [self.zmin, self.zmin + (self.nz - 1) * self.dz]
            },
            'extraction_info': {
                'hub_height_agl_m': hub_height,
                'reference_speed_ms': reference_speed,
                'num_turbines': len(winds)
            },
            'turbines': []
        }
        
        for i, wind in enumerate(winds):
            turbine_data = {
                'id': i,
                'location': {'x': wind['x'], 'y': wind['y']},
                'terrain_elevation_m': wind['z'] - hub_height,
                'hub_elevation_m': wind['z'],
                'velocity': {
                    'u_ms': wind['u'],
                    'v_ms': wind['v'],
                    'speed_ms': wind['speed'],
                    'direction_deg': wind['direction']
                }
            }
            if reference_speed is not None:
                ratio = wind['speed'] / reference_speed if reference_speed > 0 else 1.0
                turbine_data['speedup_ratio'] = ratio
            
            data['turbines'].append(turbine_data)
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"✓ Exported wind data for {len(winds)} turbines to {output_file}")
    
    def export_to_dict(self, turbine_locations: List[Tuple[float, float]], 
                      hub_height: float,
                      reference_speed: Optional[float] = None) -> Dict:
        """
        Export wind data as Python dictionary (no file written).
        
        Useful for programmatic access to wind data.
        
        Parameters:
            turbine_locations: List of (x, y) tuples
            hub_height (float): Hub height AGL in meters
            reference_speed (float, optional): If provided, compute and save speed-up ratio
        
        Returns:
            dict: Wind data for all turbines
        """
        winds = self.get_wind_at_turbines(turbine_locations, hub_height)
        
        data = {
            'solver_info': {
                'nx': self.nx, 'ny': self.ny, 'nz': self.nz,
                'dx_m': self.dx, 'dy_m': self.dy, 'dz_m': self.dz
            },
            'turbines': {}
        }
        
        for i, wind in enumerate(winds):
            data['turbines'][i] = wind.copy()
            if reference_speed is not None:
                data['turbines'][i]['speedup_ratio'] = (
                    wind['speed'] / reference_speed if reference_speed > 0 else 1.0
                )
        
        return data
    
    def get_speed_map_2d(self, height: float) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """
        Get 2D wind speed map at a specific height.
        
        Parameters:
            height (float): Height AGL in meters
        
        Returns:
            (speed_map, x_coords, y_coords) tuple where:
                - speed_map: 2D array of wind speeds (ny, nx)
                - x_coords: 1D array of x coordinates
                - y_coords: 1D array of y coordinates
        """
        # Find closest z-level to requested height
        # Note: This is absolute height, we need terrain-aware extraction
        speed_map = np.zeros((self.ny, self.nx))
        
        for j in range(self.ny):
            for i in range(self.nx):
                x = self.grid_x[i]
                y = self.grid_y[j]
                z_terrain = self.terrain[j, i]
                z_abs = z_terrain + height
                
                try:
                    wind = self.get_wind_at_point(x, y, z_abs)
                    speed_map[j, i] = wind['speed']
                except ValueError:
                    # Point outside domain
                    speed_map[j, i] = np.nan
        
        return speed_map, self.grid_x, self.grid_y


def quick_export(wind_solver, turbine_locations: List[Tuple[float, float]], 
                hub_height: float = 90.0,
                output_file: str = "floris_wind.csv",
                reference_speed: Optional[float] = None) -> Dict:
    """
    Quick convenience function to export wind data in one call.
    
    This is the simplest interface for typical use cases.
    
    Parameters:
        wind_solver: Solved WindSolver instance
        turbine_locations: List of (x, y) tuples
        hub_height (float): Hub height AGL in meters (default 90m)
        output_file (str): CSV output filename
        reference_speed (float, optional): Reference wind speed for speed-up ratio
    
    Returns:
        dict: Wind data for all turbines
    
    Example:
        from wind_solver import WindSolver
        from floris_coupling import quick_export
        
        wind = WindSolver("inputs.i")
        wind.solve()
        
        turbines = [(100, 200), (300, 400)]
        wind_data = quick_export(wind, turbines, hub_height=90.0, 
                                output_file="farm_wind.csv")
        
        wind.finalize()
    """
    wind_map = FLORISWindMap(wind_solver)
    wind_data = wind_map.export_to_dict(turbine_locations, hub_height, reference_speed)
    wind_map.export_to_csv(turbine_locations, hub_height, output_file, reference_speed)
    
    return wind_data
