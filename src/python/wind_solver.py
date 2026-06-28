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
import os
import json
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
        
        # Heat source tracking for fire coupling
        self.heat_source = None
        self.heat_source_grid_info = None
        
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
    
    def add_heat_source(self, heat_flux, grid_info=None):
        """
        Add a heat source to the wind solver for two-way coupling with fire solvers.
        
        This method stores heat flux data (from external fire solvers like wildfire_levelset)
        that affects the wind field computation in the next solve() call. The heat source
        is applied as a buoyancy forcing that creates updrafts in the wind field.
        
        Parameters:
            heat_flux (ndarray): 2D array of heat flux with shape (ny, nx).
                                Heat flux typically in units of K/s or W/m².
                                Can represent sensible heat release from fire.
            grid_info (dict, optional): Dictionary with grid metadata:
                - 'xmin', 'xmax', 'ymin', 'ymax': Domain bounds (must match wind solver)
                - 'dx', 'dy': Grid spacing (must match wind solver)
                - 'scaling_factor': Convert heat flux to velocity perturbation scale
                                   Default: 1.0 (no scaling)
                - 'temporal_decay': Decay factor for heat source over time (0-1)
                                   Default: 1.0 (no decay)
        
        Returns:
            bool: True if heat source was successfully added
        
        Raises:
            RuntimeError: If solver not initialized or heat_flux shape is invalid
        
        Example:
            # From fire solver
            fire.step()
            fluxes = fire.get_surface_fluxes()
            heat = fluxes['heat_flux']  # Shape (ny, nx)
            
            # Pass to wind solver
            wind.add_heat_source(heat)
            
            # Heat affects the next wind solve
            wind.solve()
        
        Note:
            The heat source is stored internally and cleared after solve() is called.
            This supports iterative coupling where heat is extracted from fire at each
            timestep and fed back to wind before the next solve.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized. Call initialize() first.")
        
        # Validate heat_flux
        if not isinstance(heat_flux, np.ndarray):
            heat_flux = np.asarray(heat_flux, dtype=np.float64)
        
        # Check shape compatibility
        if heat_flux.shape != (self.ny, self.nx):
            raise RuntimeError(
                f"Heat flux shape {heat_flux.shape} doesn't match grid ({self.ny}, {self.nx}). "
                f"Heat source must be 2D with shape (ny={self.ny}, nx={self.nx})."
            )
        
        # Store heat source
        self.heat_source = heat_flux.copy()
        self.heat_source_grid_info = grid_info if grid_info is not None else {}
        
        # Extract optional parameters from grid_info
        scaling = self.heat_source_grid_info.get('scaling_factor', 1.0)
        decay = self.heat_source_grid_info.get('temporal_decay', 1.0)
        
        # Report to user
        print(f"✓ Heat source added for two-way coupling")
        print(f"  Heat flux range: [{self.heat_source.min():.3e}, {self.heat_source.max():.3e}]")
        print(f"  Grid: ({self.ny}, {self.nx})")
        print(f"  Scaling: {scaling}, Decay: {decay}")
        
        return True
    
    def clear_heat_source(self):
        """
        Clear any stored heat source.
        
        This is called automatically after solve() but can be used to manually
        reset heat sources if needed.
        
        Returns:
            bool: True if heat source was cleared
        """
        self.heat_source = None
        self.heat_source_grid_info = None
        return True
    
    def get_heat_source(self):
        """
        Get the currently stored heat source (if any).
        
        Returns:
            dict: Dictionary with:
                - 'heat_flux': The 2D heat flux array (or None if not set)
                - 'grid_info': The grid metadata dictionary
                - 'is_active': Boolean indicating if heat source is active
        """
        return {
            'heat_flux': self.heat_source,
            'grid_info': self.heat_source_grid_info,
            'is_active': self.heat_source is not None
        }
    
    def add_turbine(self, x, y, hub_height, rotor_diameter, default_ct=0.8, power_curve_file="", yaw=0.0, orientation=0.0):
        """
        Add a wind turbine to the solver.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        return pyWindSolver.add_turbine(x, y, hub_height, rotor_diameter, default_ct, power_curve_file, yaw, orientation)
    
    def clear_turbines(self):
        """
        Clear all wind turbines from the solver.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        pyWindSolver.clear_turbines()
    
    def get_turbine_power_outputs(self):
        """
        Get the computed power outputs from all wind turbines.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        return pyWindSolver.get_turbine_power_outputs()
    
    def get_turbine_inflow_speeds(self):
        """
        Get the computed inflow wind speeds at all wind turbines.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        return pyWindSolver.get_turbine_inflow_speeds()

    def get_turbine_yaws(self):
        """
        Get the yaw angles from all wind turbines.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        return pyWindSolver.get_turbine_yaws()

    def get_turbine_orientations(self):
        """
        Get the orientation angles from all wind turbines.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        return pyWindSolver.get_turbine_orientations()

    def get_turbine_u_hubs(self):
        """
        Get the u_hub components of all wind turbines.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        return pyWindSolver.get_turbine_u_hubs()

    def get_turbine_v_hubs(self):
        """
        Get the v_hub components of all wind turbines.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        return pyWindSolver.get_turbine_v_hubs()

    def get_turbine_z_terrains(self):
        """
        Get the terrain elevations under all wind turbines.
        """
        if not self.initialized:
            raise RuntimeError("Solver not initialized.")
        return pyWindSolver.get_turbine_z_terrains()
    
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


class ChemicalDatabase:
    """
    A lookup database for chemical/physical properties and regulatory thresholds.
    """
    def __init__(self, json_path=None):
        if json_path is None:
            # Look in the same directory as this file
            dir_path = os.path.dirname(os.path.abspath(__file__))
            json_path = os.path.join(dir_path, "chemical_database.json")
            
        self.json_path = json_path
        self.db = {}
        if os.path.exists(json_path):
            try:
                with open(json_path, 'r') as f:
                    self.db = json.load(f)
            except Exception:
                # Fallback to embedded dictionary
                self._load_fallback_db()
        else:
            self._load_fallback_db()
            # Try to write fallback db to path for future lookups
            try:
                with open(json_path, 'w') as f:
                    json.dump(self.db, f, indent=4)
            except Exception:
                pass

    def _load_fallback_db(self):
        self.db = {
            "chlorine": {
                "name": "Chlorine",
                "molecular_weight": 70.906,
                "boiling_point": 239.11,
                "vapor_pressure": 678000.0,
                "aegl_1_1h": 0.5,
                "aegl_2_1h": 2.0,
                "aegl_3_1h": 20.0,
                "erpg_1": 1.0,
                "erpg_2": 3.0,
                "erpg_3": 20.0,
                "pac_1": 0.5,
                "pac_2": 2.0,
                "pac_3": 20.0,
                "teel_1": 0.5,
                "teel_2": 2.0,
                "teel_3": 20.0,
                "lfl": 0.0
            },
            "ammonia": {
                "name": "Ammonia",
                "molecular_weight": 17.031,
                "boiling_point": 239.82,
                "vapor_pressure": 857000.0,
                "aegl_1_1h": 30.0,
                "aegl_2_1h": 160.0,
                "aegl_3_1h": 1100.0,
                "erpg_1": 25.0,
                "erpg_2": 150.0,
                "erpg_3": 750.0,
                "pac_1": 30.0,
                "pac_2": 160.0,
                "pac_3": 1100.0,
                "teel_1": 30.0,
                "teel_2": 160.0,
                "teel_3": 1100.0,
                "lfl": 15.0
            },
            "sulfur_dioxide": {
                "name": "Sulfur Dioxide",
                "molecular_weight": 64.066,
                "boiling_point": 263.15,
                "vapor_pressure": 330000.0,
                "aegl_1_1h": 0.20,
                "aegl_2_1h": 0.75,
                "aegl_3_1h": 30.0,
                "erpg_1": 0.30,
                "erpg_2": 3.0,
                "erpg_3": 25.0,
                "pac_1": 0.20,
                "pac_2": 0.75,
                "pac_3": 30.0,
                "teel_1": 0.20,
                "teel_2": 0.75,
                "teel_3": 30.0,
                "lfl": 0.0
            },
            "benzene": {
                "name": "Benzene",
                "molecular_weight": 78.11,
                "boiling_point": 353.20,
                "vapor_pressure": 10000.0,
                "aegl_1_1h": 52.0,
                "aegl_2_1h": 800.0,
                "aegl_3_1h": 4000.0,
                "erpg_1": 50.0,
                "erpg_2": 150.0,
                "erpg_3": 1000.0,
                "pac_1": 52.0,
                "pac_2": 800.0,
                "pac_3": 4000.0,
                "teel_1": 52.0,
                "teel_2": 800.0,
                "teel_3": 4000.0,
                "lfl": 1.2
            },
            "hydrogen_fluoride": {
                "name": "Hydrogen Fluoride",
                "molecular_weight": 20.006,
                "boiling_point": 292.65,
                "vapor_pressure": 122000.0,
                "aegl_1_1h": 1.0,
                "aegl_2_1h": 24.0,
                "aegl_3_1h": 44.0,
                "erpg_1": 2.0,
                "erpg_2": 20.0,
                "erpg_3": 50.0,
                "pac_1": 1.0,
                "pac_2": 24.0,
                "pac_3": 44.0,
                "teel_1": 1.0,
                "teel_2": 24.0,
                "teel_3": 44.0,
                "lfl": 0.0
            },
            "hydrogen_cyanide": {
                "name": "Hydrogen Cyanide",
                "molecular_weight": 27.025,
                "boiling_point": 299.15,
                "vapor_pressure": 82000.0,
                "aegl_1_1h": 2.5,
                "aegl_2_1h": 7.1,
                "aegl_3_1h": 21.0,
                "erpg_1": 3.0,
                "erpg_2": 10.0,
                "erpg_3": 25.0,
                "pac_1": 2.5,
                "pac_2": 7.1,
                "pac_3": 21.0,
                "teel_1": 2.0,
                "teel_2": 7.1,
                "teel_3": 21.0,
                "lfl": 5.6
            },
            "phosgene": {
                "name": "Phosgene",
                "molecular_weight": 98.92,
                "boiling_point": 281.35,
                "vapor_pressure": 161000.0,
                "aegl_1_1h": 0.02,
                "aegl_2_1h": 0.2,
                "aegl_3_1h": 0.75,
                "erpg_1": 0.1,
                "erpg_2": 0.2,
                "erpg_3": 1.0,
                "pac_1": 0.02,
                "pac_2": 0.2,
                "pac_3": 0.75,
                "teel_1": 0.02,
                "teel_2": 0.2,
                "teel_3": 0.75,
                "lfl": 0.0
            },
            "methane": {
                "name": "Methane",
                "molecular_weight": 16.04,
                "boiling_point": 111.66,
                "vapor_pressure": 4000000.0,
                "aegl_1_1h": 65000.0,
                "aegl_2_1h": 230000.0,
                "aegl_3_1h": 400000.0,
                "erpg_1": 10000.0,
                "erpg_2": 50000.0,
                "erpg_3": 100000.0,
                "pac_1": 65000.0,
                "pac_2": 230000.0,
                "pac_3": 400000.0,
                "teel_1": 65000.0,
                "teel_2": 230000.0,
                "teel_3": 400000.0,
                "lfl": 5.0
            },
            "propane": {
                "name": "Propane",
                "molecular_weight": 44.1,
                "boiling_point": 231.05,
                "vapor_pressure": 853000.0,
                "aegl_1_1h": 5500.0,
                "aegl_2_1h": 17000.0,
                "aegl_3_1h": 33000.0,
                "erpg_1": 10000.0,
                "erpg_2": 17000.0,
                "erpg_3": 33000.0,
                "pac_1": 5500.0,
                "pac_2": 17000.0,
                "pac_3": 33000.0,
                "teel_1": 5500.0,
                "teel_2": 17000.0,
                "teel_3": 33000.0,
                "lfl": 2.1
            },
            "hydrogen_sulfide": {
                "name": "Hydrogen Sulfide",
                "molecular_weight": 34.08,
                "boiling_point": 212.85,
                "vapor_pressure": 2000000.0,
                "aegl_1_1h": 0.75,
                "aegl_2_1h": 20.0,
                "aegl_3_1h": 50.0,
                "erpg_1": 0.1,
                "erpg_2": 30.0,
                "erpg_3": 100.0,
                "pac_1": 0.75,
                "pac_2": 20.0,
                "pac_3": 50.0,
                "teel_1": 0.51,
                "teel_2": 20.0,
                "teel_3": 50.0,
                "lfl": 4.0
            },
            "methyl_isocyanate": {
                "name": "Methyl Isocyanate",
                "molecular_weight": 57.05,
                "boiling_point": 312.25,
                "vapor_pressure": 45000.0,
                "aegl_1_1h": 0.025,
                "aegl_2_1h": 0.13,
                "aegl_3_1h": 0.40,
                "erpg_1": 0.025,
                "erpg_2": 0.15,
                "erpg_3": 0.50,
                "pac_1": 0.025,
                "pac_2": 0.13,
                "pac_3": 0.40,
                "teel_1": 0.025,
                "teel_2": 0.13,
                "teel_3": 0.40,
                "lfl": 5.3
            }
        }

    def lookup(self, name):
        """
        Lookup chemical properties by name (case-insensitive).
        
        Parameters:
            name (str): Name of the chemical.
            
        Returns:
            dict: Dictionary of chemical properties, or None if not found.
        """
        key = name.lower().replace(" ", "_")
        return self.db.get(key, None)

