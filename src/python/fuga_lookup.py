#!/usr/bin/env python3
"""
fuga_lookup.py - Pre-computed Linearized Wake Lookup (Fuga-style)

This module implements a pre-computed linearized wake lookup capability mapped
onto the AMReX terrain mesh. It bypasses local analytical calculations in large
wind farms by interpolating deficits from a pre-calculated 3D lookup table (LUT).
"""

import numpy as np
from typing import Dict, List, Tuple, Any


class FugaWakeLookup:
    """
    Fuga-style pre-computed linearized wake lookup table and mapper.
    """
    
    def __init__(self, x_range: Tuple[float, float] = (0.0, 20.0), nx_lut: int = 50,
                 y_range: Tuple[float, float] = (-4.0, 4.0), ny_lut: int = 40,
                 z_range: Tuple[float, float] = (-2.0, 2.0), nz_lut: int = 20):
        """
        Initialize the Fuga lookup table dimensions and pre-compute a realistic
        linearized wake deficit field.
        """
        self.x_lut = np.linspace(x_range[0], x_range[1], nx_lut)
        self.y_lut = np.linspace(y_range[0], y_range[1], ny_lut)
        self.z_lut = np.linspace(z_range[0], z_range[1], nz_lut)
        
        # Dimensions of LUT
        self.nx_lut = nx_lut
        self.ny_lut = ny_lut
        self.nz_lut = nz_lut
        
        # Grid spacing
        self.dx_nd = (x_range[1] - x_range[0]) / (nx_lut - 1)
        self.dy_nd = (y_range[1] - y_range[0]) / (ny_lut - 1)
        self.dz_nd = (z_range[1] - z_range[0]) / (nz_lut - 1)
        
        # Pre-compute deficit lookup table using a linearized Gaussian wake model
        self.lut = np.zeros((nx_lut, ny_lut, nz_lut), dtype=float)
        self._precompute_lut()
        
    def _precompute_lut(self):
        """
        Pre-compute non-dimensional velocity deficits: delta_u / U_inf
        based on non-dimensional downwind (x/D), crosswind (y/D), and vertical (z/D) distances.
        """
        # Bastankhah Gaussian parameters
        ka = 0.04  # Wake expansion coefficient
        epsilon = 0.2  # Initial wake width parameter
        
        for i, x_nd in enumerate(self.x_lut):
            if x_nd < 0.1:
                # Close to turbine, model the rotor-disk-scale deficit
                for j, y_nd in enumerate(self.y_lut):
                    for k, z_nd in enumerate(self.z_lut):
                        r_nd = np.sqrt(y_nd**2 + z_nd**2)
                        if r_nd <= 0.5:
                            self.lut[i, j, k] = 0.35  # Initial induction-style deficit
                continue
                
            # Downstream wake expansion
            sigma_nd = ka * x_nd + epsilon
            max_deficit = 1.0 - np.sqrt(max(0.0, 1.0 - 0.8 / (8.0 * sigma_nd**2)))
            
            for j, y_nd in enumerate(self.y_lut):
                for k, z_nd in enumerate(self.z_lut):
                    r_nd = np.sqrt(y_nd**2 + z_nd**2)
                    # Gaussian distribution of deficit
                    self.lut[i, j, k] = max_deficit * np.exp(-r_nd**2 / (2.0 * sigma_nd**2))
                    
    def get_deficit(self, x_nd: float, y_nd: float, z_nd: float) -> float:
        """
        Query the non-dimensional wake deficit using tri-linear interpolation.
        """
        # Clamp coordinates to table bounds
        x_clamped = min(max(x_nd, self.x_lut[0]), self.x_lut[-1])
        y_clamped = min(max(y_nd, self.y_lut[0]), self.y_lut[-1])
        z_clamped = min(max(z_nd, self.z_lut[0]), self.z_lut[-1])
        
        # Compute grid indices
        ix = (x_clamped - self.x_lut[0]) / self.dx_nd
        iy = (y_clamped - self.y_lut[0]) / self.dy_nd
        iz = (z_clamped - self.z_lut[0]) / self.dz_nd
        
        ix0 = int(np.floor(ix))
        iy0 = int(np.floor(iy))
        iz0 = int(np.floor(iz))
        
        ix1 = min(ix0 + 1, self.nx_lut - 1)
        iy1 = min(iy0 + 1, self.ny_lut - 1)
        iz1 = min(iz0 + 1, self.nz_lut - 1)
        
        fx = ix - ix0
        fy = iy - iy0
        fz = iz - iz0
        
        # Tri-linear interpolation
        c000 = self.lut[ix0, iy0, iz0]
        c100 = self.lut[ix1, iy0, iz0]
        c010 = self.lut[ix0, iy1, iz0]
        c110 = self.lut[ix1, iy1, iz0]
        c001 = self.lut[ix0, iy0, iz1]
        c101 = self.lut[ix1, iy0, iz1]
        c011 = self.lut[ix0, iy1, iz1]
        c111 = self.lut[ix1, iy1, iz1]
        
        c00 = c000 * (1 - fx) + c100 * fx
        c10 = c010 * (1 - fx) + c110 * fx
        c01 = c001 * (1 - fx) + c101 * fx
        c11 = c011 * (1 - fx) + c111 * fx
        
        c0 = c00 * (1 - fy) + c10 * fy
        c1 = c01 * (1 - fy) + c11 * fy
        
        return c0 * (1 - fz) + c1 * fz

    def map_wakes_to_mesh_explicit(
        self,
        wind_solver,
        turbines_list: List[Dict[str, Any]],
        superposition: str = "quadratic"
    ) -> Dict[str, np.ndarray]:
        """
        Explicitly map Fuga lookups onto AMReX mesh using a list of turbine locations/specs.
        
        Parameters:
            wind_solver: An initialized and solved WindSolver instance.
            turbines_list: List of dicts, e.g. [{'x': 100.0, 'y': 150.0, 'hub_height': 80.0, 'rotor_diameter': 120.0}]
            superposition: "linear" or "quadratic" (root-sum-squares of deficits)
            
        Returns:
            dict: Updated velocity components 'u', 'v', 'w' (shape: nz, ny, nx).
        """
        # Get background/uncorrected velocities and geometry
        vel = wind_solver.get_velocity0()
        u_field = np.copy(vel['u'])
        v_field = np.copy(vel['v'])
        w_field = np.copy(vel['w'])
        
        nx, ny, nz = wind_solver.nx, wind_solver.ny, wind_solver.nz
        dx, dy, dz = wind_solver.dx, wind_solver.dy, wind_solver.dz
        xmin, ymin, zmin = wind_solver.xmin, wind_solver.ymin, wind_solver.zmin
        
        terrain = wind_solver.get_terrain()  # shape: (ny, nx)
        
        # Pre-calculate cell physical coordinates
        x_phys = xmin + (np.arange(nx) + 0.5) * dx
        y_phys = ymin + (np.arange(ny) + 0.5) * dy
        z_phys = zmin + (np.arange(nz) + 0.5) * dz
        
        # Reconstruct 3D coordinate grids
        X_grid, Y_grid, Z_grid = np.meshgrid(x_phys, y_phys, z_phys, indexing='ij')
        # X_grid, Y_grid, Z_grid shape: (nx, ny, nz)
        # Transpose to match AMReX (nz, ny, nx) ordering
        X_grid = np.transpose(X_grid, (2, 1, 0))
        Y_grid = np.transpose(Y_grid, (2, 1, 0))
        Z_grid = np.transpose(Z_grid, (2, 1, 0))
        
        # Deficit accumulation arrays
        cum_deficit_sq = np.zeros_like(u_field, dtype=float)
        cum_deficit_linear = np.zeros_like(u_field, dtype=float)
        
        # Determine ambient wind direction from mean background velocity
        u_mean = np.mean(u_field)
        v_mean = np.mean(v_field)
        wind_speed = np.sqrt(u_mean**2 + v_mean**2)
        if wind_speed < 1e-5:
            wind_speed = 1.0
            u_mean = 1.0
            v_mean = 0.0
            
        u_dir = u_mean / wind_speed
        v_dir = v_mean / wind_speed
        
        for t in turbines_list:
            t_x, t_y = t['x'], t['y']
            h_h = t['hub_height']
            D = t['rotor_diameter']
            
            # 1. Rotate grid to align with turbine wind direction (downwind/crosswind coordinates)
            dx_pt = X_grid - t_x
            dy_pt = Y_grid - t_y
            
            # Downwind distance
            x_down = dx_pt * u_dir + dy_pt * v_dir
            # Crosswind distance
            y_cross = -dx_pt * v_dir + dy_pt * u_dir
            
            # Terrain-following coordinate z_agl
            # Reconstruct cell terrain array of shape (nz, ny, nx)
            terrain_3d = np.broadcast_to(terrain[np.newaxis, :, :], (nz, ny, nx))
            z_agl = Z_grid - terrain_3d
            z_vertical = z_agl - h_h
            
            # Filter downwind cells to apply wake
            wake_mask = x_down > 0.01
            
            if np.any(wake_mask):
                # Non-dimensionalize distances by rotor diameter D
                x_nd = x_down[wake_mask] / D
                y_nd = y_cross[wake_mask] / D
                z_nd = z_vertical[wake_mask] / D
                
                # Query pre-computed LUT
                # Fast vectorized lookup or loop
                deficits = np.zeros(len(x_nd))
                for idx in range(len(x_nd)):
                    deficits[idx] = self.get_deficit(x_nd[idx], y_nd[idx], z_nd[idx])
                
                if superposition == "quadratic":
                    cum_deficit_sq[wake_mask] += deficits**2
                else:
                    cum_deficit_linear[wake_mask] += deficits
                    
        # Apply superimposed deficits to background velocity
        if superposition == "quadratic":
            total_deficit = np.sqrt(cum_deficit_sq)
        else:
            total_deficit = cum_deficit_linear
            
        total_deficit = np.clip(total_deficit, 0.0, 0.95)
        
        u_field_wake = u_field * (1.0 - total_deficit)
        v_field_wake = v_field * (1.0 - total_deficit)
        
        return {
            'u': u_field_wake,
            'v': v_field_wake,
            'w': w_field
        }
