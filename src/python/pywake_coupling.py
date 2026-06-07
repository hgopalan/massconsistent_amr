#!/usr/bin/env python3
"""
pywake_coupling.py - Standalone module for exporting wind fields to PyWake

This module provides utilities to extract solved wind fields from massconsistent_amr
and format them as PyWake Site or WAsPGridSite objects.

It is designed to run independently - if PyWake is not installed, it provides
fully functional mock/fallback classes so that tests and dry-runs can execute.
"""

import os
import numpy as np
from typing import Dict, List, Tuple, Optional

try:
    from pywake.site import Site
    from pywake.site.wasp_grid_site import WAsPGridSite
    from pywake import LocalWind
    PYWAKE_AVAILABLE = True
except ImportError:
    PYWAKE_AVAILABLE = False
    # Mock classes if pywake is not available
    class Site:
        """Mock PyWake Site base class."""
        def __init__(self):
            pass

    class WAsPGridSite:
        """Mock PyWake WAsPGridSite class."""
        def __init__(self, output_dir):
            self.output_dir = output_dir
            self.is_mock = True

    class LocalWind:
        """Mock PyWake LocalWind class."""
        def __init__(self, x, y, h, wd, ws, WD_ilk, WS_ilk, TI_ilk, P_ilk):
            self.x = x
            self.y = y
            self.h = h
            self.wd = wd
            self.ws = ws
            self.WD_ilk = WD_ilk
            self.WS_ilk = WS_ilk
            self.TI_ilk = TI_ilk
            self.P_ilk = P_ilk


class MassConsistentSite(Site):
    """
    Spatially-varying PyWake Site subclass representing the resolved
    mass-consistent wind field from massconsistent_amr.
    """
    
    def __init__(self, wind_solver):
        """
        Initialize MassConsistentSite from a solved WindSolver instance.
        """
        super().__init__()
        if not wind_solver.initialized:
            raise RuntimeError("Wind solver not initialized")
        if not wind_solver.solved:
            raise RuntimeError("Wind field not solved. Call wind.solve() first.")
        
        self.wind_solver = wind_solver
        
        # Extract wind field
        vel = wind_solver.get_velocity()
        self.u_field = vel['u']  # shape: (nz, ny, nx)
        self.v_field = vel['v']
        self.w_field = vel['w']
        
        # Extract terrain
        self.terrain = wind_solver.get_terrain()  # shape: (ny, nx)
        
        # Grid dimensions and bounds
        self.nx = wind_solver.nx
        self.ny = wind_solver.ny
        self.nz = wind_solver.nz
        self.dx = wind_solver.dx
        self.dy = wind_solver.dy
        self.dz = wind_solver.dz
        
        self.xmin = wind_solver.xmin
        self.ymin = wind_solver.ymin
        self.zmin = wind_solver.zmin
        
        self.grid_x = np.linspace(self.xmin, self.xmin + (self.nx - 1) * self.dx, self.nx)
        self.grid_y = np.linspace(self.ymin, self.ymin + (self.ny - 1) * self.dy, self.ny)
        self.grid_z = np.linspace(self.zmin, self.zmin + (self.nz - 1) * self.dz, self.nz)

    def get_terrain_elevation(self, x: float, y: float) -> float:
        """
        Get terrain elevation at (x, y) using bilinear interpolation.
        """
        xmax = self.xmin + (self.nx - 1) * self.dx
        ymax = self.ymin + (self.ny - 1) * self.dy
        
        x_clamped = min(max(x, self.xmin), xmax)
        y_clamped = min(max(y, self.ymin), ymax)
        
        i_x = (x_clamped - self.xmin) / self.dx
        i_y = (y_clamped - self.ymin) / self.dy
        
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
        return float(z_terrain)

    def _interpolate_to_point(self, x: float, y: float, z: float) -> Tuple[float, float, float]:
        """
        Tri-linear interpolation of velocity (u, v, w) to a 3D point.
        """
        xmax = self.xmin + (self.nx - 1) * self.dx
        ymax = self.ymin + (self.ny - 1) * self.dy
        zmax = self.zmin + (self.nz - 1) * self.dz
        
        x_clamped = min(max(x, self.xmin), xmax)
        y_clamped = min(max(y, self.ymin), ymax)
        z_clamped = min(max(z, self.zmin), zmax)
        
        i_x = (x_clamped - self.xmin) / self.dx
        i_y = (y_clamped - self.ymin) / self.dy
        i_z = (z_clamped - self.zmin) / self.dz
        
        i0_x = int(np.floor(i_x))
        i0_y = int(np.floor(i_y))
        i0_z = int(np.floor(i_z))
        
        i1_x = min(i0_x + 1, self.nx - 1)
        i1_y = min(i0_y + 1, self.ny - 1)
        i1_z = min(i0_z + 1, self.nz - 1)
        
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

        # Tri-linear interpolation for w
        w000 = self.w_field[i0_z, i0_y, i0_x]
        w100 = self.w_field[i0_z, i0_y, i1_x]
        w010 = self.w_field[i0_z, i1_y, i0_x]
        w110 = self.w_field[i0_z, i1_y, i1_x]
        w001 = self.w_field[i1_z, i0_y, i0_x]
        w101 = self.w_field[i1_z, i0_y, i1_x]
        w011 = self.w_field[i1_z, i1_y, i0_x]
        w111 = self.w_field[i1_z, i1_y, i1_x]
        
        w_interp = (
            w000 * (1 - fx) * (1 - fy) * (1 - fz) +
            w100 * fx * (1 - fy) * (1 - fz) +
            w010 * (1 - fx) * fy * (1 - fz) +
            w110 * fx * fy * (1 - fz) +
            w001 * (1 - fx) * (1 - fy) * fz +
            w101 * fx * (1 - fy) * fz +
            w011 * (1 - fx) * fy * fz +
            w111 * fx * fy * fz
        )
        
        return float(u_interp), float(v_interp), float(w_interp)

    def local_wind(self, x, y, h, wd=None, ws=None, **kwargs):
        """
        Extract local wind speed, direction, and variables at coordinates.
        Supports PyWake format where:
            x, y, h: arrays/lists of positions (h is AGL height)
            wd: requested wind directions (defaults to solved wind solver direction if None)
            ws: requested wind speeds (defaults to solved wind solver reference speed if None)
        """
        x = np.atleast_1d(x)
        y = np.atleast_1d(y)
        h = np.atleast_1d(h)
        
        if len(h) == 1 and len(x) > 1:
            h = np.full_like(x, h[0])
            
        num_turbines = len(x)
        
        # Get reference values
        ref_u = getattr(self.wind_solver, 'U_ref', 10.0)
        ref_v = getattr(self.wind_solver, 'V_ref', 0.0)
        ref_speed = np.sqrt(ref_u**2 + ref_v**2)
        ref_dir = np.degrees(np.arctan2(ref_u, ref_v)) % 360.0
        
        if wd is None:
            wd = np.atleast_1d(ref_dir)
        else:
            wd = np.atleast_1d(wd)
            
        if ws is None:
            ws = np.atleast_1d(ref_speed)
        else:
            ws = np.atleast_1d(ws)
            
        # Shape: (num_turbines, num_wd, num_ws)
        WD_ilk = np.zeros((num_turbines, len(wd), len(ws)))
        WS_ilk = np.zeros((num_turbines, len(wd), len(ws)))
        TI_ilk = np.full((num_turbines, len(wd), len(ws)), 0.1)
        P_ilk = np.full((num_turbines, len(wd), len(ws)), 1.0 / (len(wd) * len(ws)))
        
        for i in range(num_turbines):
            z_terrain = self.get_terrain_elevation(x[i], y[i])
            z_abs = z_terrain + h[i]
            
            u, v, w = self._interpolate_to_point(x[i], y[i], z_abs)
            solved_speed = np.sqrt(u**2 + v**2)
            solved_dir = np.degrees(np.arctan2(u, v)) % 360.0
            
            for iw, target_wd in enumerate(wd):
                for ispd, target_ws in enumerate(ws):
                    scale_factor = target_ws / ref_speed if ref_speed > 0 else 1.0
                    scaled_speed = solved_speed * scale_factor
                    
                    dir_shift = target_wd - ref_dir
                    final_dir = (solved_dir + dir_shift) % 360.0
                    
                    WD_ilk[i, iw, ispd] = final_dir
                    WS_ilk[i, iw, ispd] = scaled_speed
                    
        return LocalWind(x, y, h, wd, ws, WD_ilk, WS_ilk, TI_ilk, P_ilk)


def export_to_wasp_grd(filename: str, data: np.ndarray, xmin: float, xmax: float, ymin: float, ymax: float):
    """
    Export a 2D numpy array to a WAsP/Surfer ASCII Grid (.grd) file.
    """
    ny, nx = data.shape
    zmin = np.nanmin(data)
    zmax = np.nanmax(data)
    if np.isnan(zmin):
        zmin = 0.0
    if np.isnan(zmax):
        zmax = 0.0
        
    with open(filename, 'w') as f:
        f.write("DSAA\n")
        f.write(f"{nx} {ny}\n")
        f.write(f"{xmin} {xmax}\n")
        f.write(f"{ymin} {ymax}\n")
        f.write(f"{zmin} {zmax}\n")
        
        for j in range(ny):
            row_str = " ".join(f"{val:.6f}" for val in data[j, :])
            f.write(row_str + "\n")


def to_wasp_grid_site(wind_solver, height_agl: float, output_dir: str):
    """
    Export wind solver fields to WAsP GRD files and format/return a WAsPGridSite.
    """
    os.makedirs(output_dir, exist_ok=True)
    
    # Extract terrain and compute roughness
    terrain = wind_solver.get_terrain()
    z0_val = getattr(wind_solver, 'z0', 0.1)
    roughness = np.full_like(terrain, z0_val)
    
    # Extract wind components at selected AGL height
    vel_agl = wind_solver.get_velocity_at_agl(height_agl)
    u = vel_agl['u']
    v = vel_agl['v']
    speed = np.sqrt(u**2 + v**2)
    direction = np.degrees(np.arctan2(u, v)) % 360.0
    
    # Paths for GRD output files
    elevation_file = os.path.join(output_dir, "elevation.grd")
    roughness_file = os.path.join(output_dir, "roughness.grd")
    speed_file = os.path.join(output_dir, "wind_speed.grd")
    direction_file = os.path.join(output_dir, "wind_direction.grd")
    
    # Export all grids
    export_to_wasp_grd(elevation_file, terrain, wind_solver.xmin, wind_solver.xmax, wind_solver.ymin, wind_solver.ymax)
    export_to_wasp_grd(roughness_file, roughness, wind_solver.xmin, wind_solver.xmax, wind_solver.ymin, wind_solver.ymax)
    export_to_wasp_grd(speed_file, speed, wind_solver.xmin, wind_solver.xmax, wind_solver.ymin, wind_solver.ymax)
    export_to_wasp_grd(direction_file, direction, wind_solver.xmin, wind_solver.xmax, wind_solver.ymin, wind_solver.ymax)
    
    print(f"✓ WAsP GRD files successfully written to {output_dir}")
    
    if PYWAKE_AVAILABLE:
        try:
            return WAsPGridSite(output_dir)
        except Exception as e:
            print(f"Warning: Failed to instantiate actual WAsPGridSite: {e}. Returning fallback mock object.")
            return MockWAsPGridSite(output_dir)
    else:
        return MockWAsPGridSite(output_dir)


class MockWAsPGridSite(WAsPGridSite):
    """Fallback Mock class for WAsPGridSite if PyWake is not installed."""
    def __init__(self, output_dir):
        super().__init__(output_dir)
        self.output_dir = output_dir
        self.is_mock = True
