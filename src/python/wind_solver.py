#!/usr/bin/env python3
"""
wind_solver.py - High-level Python wrapper for pyWindSolver mass-consistent wind solver

Provides a clean, object-oriented API for running mass-consistent wind simulations from Python.
Supports coupling with external fire solvers (e.g., wildfire_levelset).

Example:
    from wind_solver import WindSolver
    
    # Initialize and solve
    wind = WindSolver("inputs.i")
    wind.solve()
    
    # Extract velocity at 10m AGL
    vel_agl = wind.get_velocity_at_agl(10.0)
    
    # Save results
    wind.write_plotfile("plt_wind")
    wind.finalize()
"""

import numpy as np
try:
    import pyWindSolver
except ImportError as e:
    raise ImportError(
        "Could not import pyWindSolver module. "
        "Build with: cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON\n"
        f"Error: {e}"
    )


class WindSolver:
    """
    High-level Python interface to massconsistent_amr wind solver.
    
    This class provides a convenient object-oriented API for solving
    mass-consistent wind fields over terrain, with support for coupling
    to external fire solvers.
    
    Attributes:
        initialized (bool): Whether the solver has been initialized
        solved (bool): Whether the wind field has been solved
        iters (int): Number of MLMG iterations in last solve
        residual (float): Final residual from MLMG solve
        nx, ny, nz (int): Grid dimensions
        xmin, xmax, ymin, ymax, zmin, zmax (float): Domain bounds (meters)
        dx, dy, dz (float): Cell sizes (meters)
        zs_min, zs_max (float): Terrain elevation bounds (meters)
    """
    
    def __init__(self, inputs_file=None):
        """
        Initialize the wind solver.
        
        Parameters:
            inputs_file (str, optional): Path to inputs file.
                                        If None, must call initialize() later.
        """
        self.initialized = False
        self.solved = False
        self.iters = 0
        self.residual = 0.0
        self.nx = self.ny = self.nz = 0
        self.xmin = self.xmax = self.ymin = self.ymax = self.zmin = self.zmax = 0.0
        self.dx = self.dy = self.dz = 0.0
        self.zs_min = self.zs_max = 0.0
        
        if inputs_file is not None:
            self.initialize(inputs_file)
    
    def initialize(self, inputs_file):
        """
        Initialize the solver from an inputs file.
        
        Parameters:
            inputs_file (str): Path to the inputs file (e.g., "inputs.i")
        
        Returns:
            bool: True if initialization succeeded
        
        Raises:
            RuntimeError: If initialization fails
        """
        result = pyWindSolver.initialize(inputs_file)
        
        if not result['success']:
            raise RuntimeError(f"Failed to initialize wind solver from {inputs_file}")
        
        self.initialized = True
        self.nx = result['nx']
        self.ny = result['ny']
        self.nz = result['nz']
        self.xmin = result['xmin']
        self.xmax = result['xmax']
        self.ymin = result['ymin']
        self.ymax = result['ymax']
        self.zmin = result['zmin']
        self.zmax = result['zmax']
        self.dx = result['dx']
        self.dy = result['dy']
        self.dz = result['dz']
        
        # Get terrain bounds
        terrain_bounds = pyWindSolver.get_terrain_bounds()
        self.zs_min = terrain_bounds['zs_min']
        self.zs_max = terrain_bounds['zs_max']
        
        print(f"✓ Wind solver initialized")
        print(f"  Grid: {self.nx} × {self.ny} × {self.nz}")
        print(f"  Domain: X=[{self.xmin:.1f}, {self.xmax:.1f}] m, "
              f"Y=[{self.ymin:.1f}, {self.ymax:.1f}] m, "
              f"Z=[{self.zmin:.1f}, {self.zmax:.1f}] m")
        print(f"  Resolution: dx={self.dx:.2f} m, dy={self.dy:.2f} m, dz={self.dz:.2f} m")
        print(f"  Terrain: [{self.zs_min:.1f}, {self.zs_max:.1f}] m")
        
        return True
    
    def solve(self):
        """
        Solve for the mass-consistent wind field.
        
        Returns:
            dict: Dictionary with 'success', 'solved', 'iters', 'residual'
        
        Raises:
            RuntimeError: If solver is not initialized or solve fails
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized. Call initialize() first.")
        
        print("Solving for mass-consistent wind field...")
        result = pyWindSolver.solve()
        
        if not result['success']:
            raise RuntimeError("Wind solve failed")
        
        self.solved = result['solved']
        self.iters = result['iters']
        self.residual = result['residual']
        
        print(f"✓ Wind solve complete")
        print(f"  MLMG iterations: {self.iters}")
        print(f"  Final residual: {self.residual:.2e}")
        
        return result
    
    def get_status(self):
        """
        Get solver status.
        
        Returns:
            dict: Dictionary with 'solved', 'iters', 'residual'
        """
        return pyWindSolver.get_status()
    
    def get_velocity(self):
        """
        Get the corrected (mass-consistent) velocity field.
        
        Returns:
            dict: Dictionary with 'u', 'v', 'w' numpy arrays (shape: nz, ny, nx)
        
        Raises:
            RuntimeError: If solver is not initialized
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        
        return pyWindSolver.get_velocity()
    
    def get_velocity0(self):
        """
        Get the initial (uncorrected) velocity field.
        
        Returns:
            dict: Dictionary with 'u', 'v', 'w' numpy arrays (shape: nz, ny, nx)
        
        Raises:
            RuntimeError: If solver is not initialized
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        
        return pyWindSolver.get_velocity0()
    
    def get_lambda(self):
        """
        Get the Lagrange multiplier field.
        
        Returns:
            ndarray: 3D array (shape: nz, ny, nx) of Lagrange multiplier values
        
        Raises:
            RuntimeError: If solver is not initialized
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        
        return pyWindSolver.get_lambda()
    
    def get_div0(self):
        """
        Get the divergence of the initial velocity field.
        
        Returns:
            ndarray: 3D array (shape: nz, ny, nx) of divergence values
        
        Raises:
            RuntimeError: If solver is not initialized
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        
        return pyWindSolver.get_div0()
    
    def get_terrain(self):
        """
        Get the terrain elevation field.
        
        Returns:
            ndarray: 2D array (shape: ny, nx) of terrain elevations
        
        Raises:
            RuntimeError: If solver is not initialized
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        
        return pyWindSolver.get_terrain()
    
    def get_velocity_at_agl(self, agl_height):
        """
        Extract velocity at a specific height above ground level (AGL).
        
        Parameters:
            agl_height (float): Height above ground level in meters
        
        Returns:
            dict: Dictionary with 'u', 'v', 'w' numpy arrays (shape: ny, nx) and 'agl'
        
        Raises:
            RuntimeError: If solver is not initialized
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        
        return pyWindSolver.get_velocity_at_agl(agl_height)
    
    def get_velocity_at_k(self, k):
        """
        Extract velocity at a specific k-index (vertical level).
        
        Parameters:
            k (int): Vertical level index (0 = lowest level)
        
        Returns:
            dict: Dictionary with 'u', 'v', 'w' numpy arrays (shape: ny, nx) and 'k'
        
        Raises:
            RuntimeError: If solver is not initialized
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        
        return pyWindSolver.get_velocity_at_k(k)
    
    def update_reference_wind(self, U_ref, V_ref):
        """
        Update the reference wind and re-initialize the velocity field.
        
        Parameters:
            U_ref (float): Reference wind x-component (m/s)
            V_ref (float): Reference wind y-component (m/s)
        
        Returns:
            bool: True on success
        
        Raises:
            RuntimeError: If update fails
        """
        success = pyWindSolver.update_reference_wind(U_ref, V_ref)
        if not success:
            raise RuntimeError("Failed to update reference wind")
        
        self.solved = False  # Need to re-solve
        print(f"✓ Reference wind updated: U={U_ref} m/s, V={V_ref} m/s")
        return success
    
    def update_parameters(self, alpha_h=None, alpha_v=None, tol_rel=None, max_iter=None):
        """
        Update solver parameters.
        
        Parameters:
            alpha_h (float, optional): Horizontal anisotropy factor
            alpha_v (float, optional): Vertical anisotropy factor
            tol_rel (float, optional): Relative tolerance for MLMG solver
            max_iter (int, optional): Maximum iterations for MLMG solver
        
        Returns:
            bool: True on success
        
        Raises:
            RuntimeError: If update fails
        """
        # Use existing values as defaults
        if alpha_h is None:
            alpha_h = 1.0  # Default
        if alpha_v is None:
            alpha_v = 1.0  # Default
        if tol_rel is None:
            tol_rel = 1.0e-8  # Default
        if max_iter is None:
            max_iter = 200  # Default
        
        success = pyWindSolver.update_parameters(alpha_h, alpha_v, tol_rel, max_iter)
        if not success:
            raise RuntimeError("Failed to update solver parameters")
        
        self.solved = False  # Need to re-solve
        print(f"✓ Solver parameters updated")
        return success
    
    def write_plotfile(self, plotfile_name="plt_wind"):
        """
        Write AMReX plotfile.
        
        Parameters:
            plotfile_name (str): Plotfile name/prefix
        
        Returns:
            bool: True on success
        
        Raises:
            RuntimeError: If write fails
        """
        success = pyWindSolver.write_plotfile(plotfile_name)
        if not success:
            raise RuntimeError(f"Failed to write plotfile {plotfile_name}")
        
        print(f"✓ Wrote plotfile: {plotfile_name}")
        return success
    
    def write_extract(self, extract_filename="wind_extract.csv", agl_height=10.0):
        """
        Write terrain-aligned CSV extract at specified AGL height.
        
        Parameters:
            extract_filename (str): Output CSV filename
            agl_height (float): Height above ground level in meters
        
        Returns:
            bool: True on success
        
        Raises:
            RuntimeError: If write fails
        """
        success = pyWindSolver.write_extract(extract_filename, agl_height)
        if not success:
            raise RuntimeError(f"Failed to write extract {extract_filename}")
        
        print(f"✓ Wrote extract: {extract_filename} (AGL={agl_height} m)")
        return success
    
    def finalize(self):
        """
        Clean up and finalize the wind solver.
        """
        if self.initialized:
            pyWindSolver.finalize()
            self.initialized = False
            self.solved = False
            print("✓ Wind solver finalized")
    
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - ensures finalize is called."""
        self.finalize()
        return False
    
    def __del__(self):
        """Destructor - ensures cleanup."""
        if self.initialized:
            try:
                self.finalize()
            except Exception:
                pass
    
    def is_initialized(self):
        """
        Check if the solver is initialized.
        
        Returns:
            bool: True if initialized
        """
        return pyWindSolver.is_initialized()
