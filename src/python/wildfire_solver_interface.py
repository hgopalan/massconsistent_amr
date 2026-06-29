#!/usr/bin/env python3
"""
wildfire_solver_interface.py - Reference implementation of WildfireSolver interface for wildfire_levelset

This module defines the comprehensive Python interface that should be exposed by wildfire_levelset
for two-way coupling with massconsistent_amr wind solver. It includes:

1. Core solver class with initialization and domain management
2. Wind-fire coupling interface (3D wind input, surface flux extraction)
3. Rate of Spread (ROS) calculation using Rothermel and Richards models
4. State management and fire front tracking
5. Fuel and environmental data I/O
6. Ignition and initial condition setup
7. Model configuration and selection
8. Output and diagnostic utilities

This interface ensures compatibility between massconsistent_amr and wildfire_levelset for:
- One-way coupling: Wind drives fire spread
- Two-way coupling: Fire heating feeds back to wind solver for fire-induced updrafts

Author: massconsistent_amr team
Date: 2026-06-28
References:
  - Rothermel, R. C. (1972). "A mathematical model for predicting fire spread in wildland fuels"
  - Richards, G. D. (1990). "An elliptical growth model of forest fire fronts and its applications to fire management"
  - Bastankhah, M., & Porté-Agel, F. (2016). "A new analytical model for wind farm power output"
"""

import numpy as np
from typing import Optional, Dict, Tuple, List, Callable
from abc import ABC, abstractmethod


class WildfireSolver(ABC):
    """
    Abstract base class for wildfire simulation solver.
    
    This class defines the complete interface that wildfire_levelset must implement
    for tight integration with massconsistent_amr wind solver. It supports:
    
    - Flexible ROS calculation models (Rothermel, Richards, or hybrid)
    - Full 3D wind field integration from wind solver
    - Two-way coupling with heat flux extraction
    - Comprehensive fuel and terrain data management
    - Advanced diagnostic and analysis capabilities
    
    Attributes:
        nx, ny (int): Horizontal grid dimensions
        xmin, xmax, ymin, ymax (float): Horizontal domain bounds (m)
        dx, dy (float): Horizontal cell sizes (m)
        time (float): Current simulation time (s)
        step (int): Current timestep number
        model_type (str): Fire spread model ('rothermel', 'richards', 'hybrid', 'levelset')
    """
    
    # ==================== INITIALIZATION AND SETUP ====================
    
    def __init__(self, inputs_file: str, model_type: str = "rothermel"):
        """
        Initialize the fire solver from an inputs file.
        
        Parameters:
            inputs_file (str): Path to fire solver inputs file (.i format)
            model_type (str): Fire spread model to use. Default: "rothermel"
                Options: "rothermel" (Rothermel 1972), "richards" (Richards 1990),
                         "hybrid" (blend both), "levelset" (level-set method)
        
        Raises:
            FileNotFoundError: If inputs_file doesn't exist
            RuntimeError: If initialization fails
        """
        self.inputs_file = inputs_file
        self.model_type = model_type.lower()
        self.initialized = False
        self.time = 0.0
        self.step = 0
        
        # Grid properties
        self.nx = 0
        self.ny = 0
        self.xmin = self.xmax = self.ymin = self.ymax = 0.0
        self.dx = self.dy = 0.0
        self.domain_area = 0.0
        
        # Fire state arrays (to be populated)
        self.phi = None  # Level-set/fire front
        self.ros = None  # Rate of spread (m/min)
        self.intensity = None  # Fireline intensity (kW/m)
        self.fuel_consumed = None  # Burned fraction
        
        # Fuel and environment data
        self.fuel_model_map = None  # Fuel model ID per cell
        self.fuel_moisture = None  # Fuel moisture (%)
        self.fuel_load = None  # Fuel load (kg/m²)
        self.slope = None  # Terrain slope (degrees)
        self.aspect = None  # Terrain aspect (degrees)
        self.elevation = None  # Elevation (m)
        
        # Wind data for coupling
        self.u_3d = None  # 3D wind u-component
        self.v_3d = None  # 3D wind v-component
        self.w_3d = None  # 3D wind w-component
        self.wind_nz = 0  # Vertical levels
        self.wind_zmin = 0.0  # Vertical bounds
        self.wind_zmax = 0.0
        
    # ==================== DOMAIN PROPERTIES ====================
    
    @property
    def domain_bounds(self) -> Dict[str, float]:
        """Get domain bounds."""
        return {
            'xmin': self.xmin, 'xmax': self.xmax,
            'ymin': self.ymin, 'ymax': self.ymax
        }
    
    @property
    def grid_spacing(self) -> Dict[str, float]:
        """Get grid cell sizes."""
        return {'dx': self.dx, 'dy': self.dy}
    
    @property
    def grid_dimensions(self) -> Dict[str, int]:
        """Get grid dimensions."""
        return {'nx': self.nx, 'ny': self.ny}
    
    # ==================== WIND-FIRE COUPLING INTERFACE ====================
    
    @abstractmethod
    def update_wind_3d(self, u: np.ndarray, v: np.ndarray, w: np.ndarray,
                       nz: int, zmin: float, zmax: float) -> None:
        """
        Update fire solver with 3D wind field from wind solver.
        
        This is the primary coupling interface for one-way and two-way coupling.
        The wind solver calls this method after solving for each timestep.
        
        Parameters:
            u (np.ndarray): 3D x-component wind velocity (shape: nz, ny, nx) in m/s
            v (np.ndarray): 3D y-component wind velocity (shape: nz, ny, nx) in m/s
            w (np.ndarray): 3D z-component wind velocity (shape: nz, ny, nx) in m/s
            nz (int): Number of vertical levels
            zmin (float): Minimum vertical height (m)
            zmax (float): Maximum vertical height (m)
        
        Implementation Requirements:
            1. Store or cache the 3D wind arrays internally
            2. Extract horizontal wind at flame height (typically 0.5-2.0 m AGL)
            3. Handle wind interpolation/projection to fire's 2D grid
            4. Consider vertical wind shear for slope/wind-enhanced ROS
            5. Validate grid alignment with wind solver domain
        
        Note:
            - Wind arrays are provided in (z, y, x) order from AMReX
            - Fire domain should be horizontal subset of wind domain
            - Coupling expects fresh 3D wind before each fire.step() call
        """
        pass
    
    @abstractmethod
    def get_surface_fluxes(self) -> Optional[Dict[str, np.ndarray]]:
        """
        Extract fire thermodynamic outputs for wind solver feedback (two-way coupling).
        
        This method is called by wind solver during two-way coupling to get heat
        sources that affect wind dynamics. The heat flux drives fire-induced updrafts.
        
        Returns:
            dict or None: Dictionary with fire surface fluxes, or None if not available.
                Keys may include:
                - 'heat_flux' (required): 2D array (ny, nx) of sensible heat flux (kW/m²)
                - 'sensible_heat': Sensible heat flux (K/s or similar)
                - 'latent_heat': Latent heat flux (evaporative cooling)
                - 'flame_height': Flame length distribution (m)
                - 'fireline_intensity': Byram's fireline intensity (kW/m)
                - 'surface_temp': Surface temperature distribution (K)
                - 'smoke_emission': Smoke/aerosol emissions (kg/m²/s)
        
        Implementation Requirements:
            1. Heat flux should peak at active fire front
            2. Should be zero in unburned and already-burned areas
            3. Should reflect both intensity and area coverage
            4. Consider flame height for vertical heat distribution
            5. Temporal variation based on fire dynamics
        
        Note:
            - Called after each fire.step() during coupled simulation
            - Heat source is added to wind solver via wind.add_heat_source()
            - Return None if method is not supported (disables two-way coupling)
        """
        pass
    
    # ==================== RATE OF SPREAD (ROS) CALCULATIONS ====================
    
    @abstractmethod
    def compute_rothermel_ros(self, fuel_model: int, moisture: np.ndarray,
                             slope: np.ndarray, wind_speed: np.ndarray,
                             wind_direction: np.ndarray) -> Dict[str, np.ndarray]:
        """
        Compute Rate of Spread using Rothermel (1972) model.
        
        Rothermel's semi-empirical model combines:
        - Fuel-dependent parameters (moisture of extinction, heat content, etc.)
        - Slope enhancement factor
        - Wind enhancement factor with directional dependence
        - Moisture damping effects
        
        Parameters:
            fuel_model (int): Rothermel fuel model 1-13 (standard NFDRS fuel models)
            moisture (np.ndarray): Fuel moisture content (%), shape (ny, nx)
            slope (np.ndarray): Terrain slope (degrees), shape (ny, nx)
            wind_speed (np.ndarray): Wind speed at flame height (m/s), shape (ny, nx)
            wind_direction (np.ndarray): Wind direction (degrees from N), shape (ny, nx)
        
        Returns:
            dict: Fire spread properties with keys:
                - 'ros_no_wind_slope' (np.ndarray): Base ROS without wind/slope (m/min)
                - 'ros_with_slope' (np.ndarray): ROS with slope enhancement only (m/min)
                - 'ros_with_wind' (np.ndarray): Final ROS with wind+slope (m/min)
                - 'fireline_intensity' (np.ndarray): Byram's fireline intensity (kW/m)
                - 'flame_length' (np.ndarray): Flame length (m)
                - 'direction_factor' (np.ndarray): Wind directional effectiveness [0, 1]
                - 'spread_direction' (np.ndarray): Direction of max spread (degrees from N)
                - 'ros_components' (dict): Optional decomposition of ROS drivers
        
        Fuel Model Details (NFDRS standard):
            1. Short grass (cured)
            2. Timber-grass-shrub
            3. Tall grass (cured)
            4. Chaparral
            5. Timber litter
            6. Conifer plantation litter
            7. Ponderosa pine/mixed conifer litter
            8. Closed timber litter
            9. Hardwood litter
            10. Timber-shrub (black spruce-lichen)
            11. Timber-shrub (light conifer-lichen)
            12. Closed shelterwood
            13. Palm-grass-shrub
        
        Implementation Notes:
            - Fuel model must be in range [1, 13]
            - ROS typically 0.1-30 m/min depending on conditions
            - Rothermel assumes elliptical fire front
            - Wind effect diminishes rapidly with high slope values
            - Moisture of extinction varies by fuel model
        
        References:
            Rothermel, R. C. (1972). "A mathematical model for predicting fire spread
            in wildland fuels." USDA Forest Service, Research Paper INT-115.
        """
        pass
    
    @abstractmethod
    def compute_richards_ros(self, fuel_load: np.ndarray, fuel_moisture: np.ndarray,
                            wind_speed: np.ndarray, slope: np.ndarray) -> Dict:
        """
        Compute Rate of Spread using Richards (1990) model.
        
        Richards' model provides explicit ROS components and is useful for:
        - Sensitivity analysis
        - Alternative parameterizations
        - Coupling with other fuel consumption models
        
        Parameters:
            fuel_load (np.ndarray): Fuel load (kg/m²), shape (ny, nx)
            fuel_moisture (np.ndarray): Fuel moisture content (%), shape (ny, nx)
            wind_speed (np.ndarray): Wind speed at flame height (m/s), shape (ny, nx)
            slope (np.ndarray): Terrain slope (degrees), shape (ny, nx)
        
        Returns:
            dict: Fire spread properties with keys:
                - 'ros' (np.ndarray): Final ROS (m/min)
                - 'ros_components' (dict): Decomposed as {
                    'u_component': x-direction ROS (m/min),
                    'v_component': y-direction ROS (m/min),
                    'slope_factor': Slope enhancement multiplier,
                    'wind_factor': Wind enhancement multiplier,
                    'base_ros': Base ROS before enhancements (m/min)
                  }
                - 'energy_release' (np.ndarray): Energy release rate (kJ/m²)
                - 'consumption_rate' (np.ndarray): Fuel consumption rate (kg/m²/min)
        
        Implementation Notes:
            - Richards model is more explicit than Rothermel
            - Better for energy-balance-based coupling
            - Provides vector ROS components
            - Less dependent on empirical fuel model tables
        
        References:
            Richards, G. D. (1990). "An elliptical growth model of forest fire fronts
            and its applications to fire management." International Journal of Wildland Fire, 1(2).
        """
        pass
    
    @abstractmethod
    def compute_hybrid_ros(self, model_params: Dict) -> Dict:
        """
        Compute ROS using hybrid or blended models.
        
        Allows switching between models spatially or temporally, or blending results.
        
        Parameters:
            model_params (dict): Configuration dictionary containing:
                - 'blend_factor' (float): Weight for Rothermel [0, 1], (1-weight) for Richards
                - 'rothermel_params' (dict): Parameters for Rothermel model
                - 'richards_params' (dict): Parameters for Richards model
                - 'switch_criteria' (str): "fire_intensity", "fuel_type", "moisture" etc.
        
        Returns:
            dict: Hybrid ROS results
        """
        pass
    
    # ==================== STATE MANAGEMENT ====================
    
    @abstractmethod
    def get_state(self) -> Dict:
        """
        Get current fire solver state.
        
        Returns:
            dict: Complete fire state with keys:
                - 'phi' (np.ndarray): Level-set or spread state, shape (ny, nx)
                - 'ros' (np.ndarray): Rate of spread (m/min), shape (ny, nx)
                - 'intensity' (np.ndarray): Fireline intensity (kW/m), shape (ny, nx)
                - 'fuel_consumed' (np.ndarray): Fraction of fuel consumed [0, 1], shape (ny, nx)
                - 'burned_area_fraction' (float): Fraction of domain burned [0, 1]
                - 'time' (float): Current simulation time (s)
                - 'step' (int): Current timestep number
                - 'active_perimeter' (float): Length of active fire front (m)
        """
        pass
    
    @abstractmethod
    def get_fuel_data(self) -> Dict:
        """
        Query fuel properties at all grid points.
        
        Returns:
            dict: Fuel data with keys:
                - 'fuel_model' (np.ndarray): Fuel model ID per cell, shape (ny, nx)
                - 'fuel_load' (np.ndarray): Fuel load (kg/m²), shape (ny, nx)
                - 'fuel_moisture' (np.ndarray): Fuel moisture (%), shape (ny, nx)
                - 'fuel_depth' (np.ndarray): Fuel depth (m), shape (ny, nx)
                - 'low_heat_content' (np.ndarray): Low heat content (kJ/kg), shape (ny, nx)
        """
        pass
    
    @abstractmethod
    def get_ros_components(self) -> Dict:
        """
        Decompose ROS into wind, slope, and base components.
        
        Useful for sensitivity analysis and understanding ROS drivers.
        
        Returns:
            dict: ROS decomposition with keys:
                - 'ros_base' (np.ndarray): Base ROS no wind/slope (m/min)
                - 'ros_wind_factor' (np.ndarray): Wind enhancement factor [>=1]
                - 'ros_slope_factor' (np.ndarray): Slope enhancement factor [>=1]
                - 'effective_wind_speed' (np.ndarray): Wind speed adjusted for slope
        """
        pass
    
    # ==================== TIME INTEGRATION ====================
    
    @abstractmethod
    def step(self, dt: Optional[float] = None) -> Dict:
        """
        Advance fire simulation by one timestep.
        
        Parameters:
            dt (float, optional): Override timestep (s). If None, uses adaptive CFL-based dt.
        
        Returns:
            dict: Timestep results with keys:
                - 'success' (bool): Whether step succeeded
                - 'dt' (float): Timestep actually taken (s)
                - 'time' (float): New simulation time (s)
                - 'max_ros' (float): Maximum ROS in domain (m/min)
                - 'avg_ros' (float): Average ROS in burn area (m/min)
                - 'cfl_satisfied' (bool): Whether CFL condition was satisfied
                - 'iterations' (int): Iterations required for convergence
        
        Raises:
            RuntimeError: If step fails
        """
        pass
    
    @abstractmethod
    def advance_to_time(self, target_time: float) -> Dict:
        """
        Advance simulation to specific time, taking multiple steps if needed.
        
        Parameters:
            target_time (float): Target simulation time (s)
        
        Returns:
            dict: Final state dictionary
        """
        pass
    
    # ==================== FUEL AND ENVIRONMENTAL DATA I/O ====================
    
    @abstractmethod
    def set_fuel_model_map(self, fuel_map: np.ndarray) -> None:
        """
        Set spatial fuel model distribution.
        
        Parameters:
            fuel_map (np.ndarray): 2D array with fuel model IDs [1-13], shape (ny, nx)
        
        Raises:
            ValueError: If fuel model IDs are out of range
        """
        pass
    
    @abstractmethod
    def set_fuel_moisture_field(self, moisture: np.ndarray,
                               fuel_type: str = "all") -> None:
        """
        Set fuel moisture content (dead/live fuel classes).
        
        Parameters:
            moisture (np.ndarray): 2D array of moisture percentages, shape (ny, nx)
            fuel_type (str): "dead", "live", or "all". Default: "all"
        """
        pass
    
    @abstractmethod
    def set_slope_field(self, slope: np.ndarray) -> None:
        """
        Set terrain slope for ROS calculations.
        
        Parameters:
            slope (np.ndarray): 2D array of slopes (degrees), shape (ny, nx)
        """
        pass
    
    @abstractmethod
    def set_aspect_field(self, aspect: np.ndarray) -> None:
        """
        Set terrain aspect for directional ROS calculations.
        
        Parameters:
            aspect (np.ndarray): 2D array of aspects (degrees from N), shape (ny, nx)
        """
        pass
    
    @abstractmethod
    def set_elevation_field(self, elevation: np.ndarray) -> None:
        """
        Set elevation for atmospheric corrections.
        
        Parameters:
            elevation (np.ndarray): 2D array of elevations (m), shape (ny, nx)
        """
        pass
    
    # ==================== IGNITION AND INITIAL CONDITIONS ====================
    
    @abstractmethod
    def set_ignition_point(self, x: float, y: float, time: float = 0.0) -> None:
        """
        Set point ignition source.
        
        Parameters:
            x (float): Ignition x-coordinate (m)
            y (float): Ignition y-coordinate (m)
            time (float): Ignition time (s). Default: 0.0
        """
        pass
    
    @abstractmethod
    def set_ignition_polygon(self, vertices: List[Tuple[float, float]],
                            time: float = 0.0) -> None:
        """
        Set polygon-shaped initial fire perimeter.
        
        Parameters:
            vertices (list): List of (x, y) tuples defining polygon (m)
            time (float): Ignition time (s). Default: 0.0
        """
        pass
    
    @abstractmethod
    def set_initial_fire_state(self, phi: np.ndarray, time: float = 0.0) -> None:
        """
        Initialize fire state from array (for restart/coupling scenarios).
        
        Parameters:
            phi (np.ndarray): Initial level-set/fire front state, shape (ny, nx)
            time (float): Initial time (s). Default: 0.0
        """
        pass
    
    # ==================== MODEL CONFIGURATION ====================
    
    @abstractmethod
    def configure_rothermel(self, model_number: int,
                           fire_direction_preference: str = "maximum_spread") -> None:
        """
        Configure Rothermel spread model.
        
        Parameters:
            model_number (int): Fuel model 1-13
            fire_direction_preference (str): "maximum_spread", "wind_direction", "ellipse_major"
        """
        pass
    
    @abstractmethod
    def configure_richards(self, coefficients: Dict[str, float]) -> None:
        """
        Configure Richards model coefficients.
        
        Parameters:
            coefficients (dict): Model parameters:
                - 'ros_0': Base ROS coefficient
                - 'wind_factor': Wind enhancement coefficient
                - 'slope_factor': Slope enhancement coefficient
                - 'moisture_response': "linear", "exponential", or "rothermel"
        """
        pass
    
    @abstractmethod
    def set_ros_calculation_method(self, method: str) -> None:
        """
        Select which ROS model to use.
        
        Parameters:
            method (str): "rothermel", "richards", "hybrid", "levelset"
        """
        pass
    
    # ==================== DOMAIN AND BOUNDARY HANDLING ====================
    
    @abstractmethod
    def set_domain_bounds(self, xmin: float, xmax: float,
                         ymin: float, ymax: float) -> None:
        """
        Define fire domain matching wind solver domain.
        
        Parameters:
            xmin, xmax, ymin, ymax (float): Domain bounds (m)
        """
        pass
    
    @abstractmethod
    def set_periodic_boundaries(self, x_periodic: bool = False,
                               y_periodic: bool = False) -> None:
        """
        Configure boundary conditions (typically non-periodic for fire).
        
        Parameters:
            x_periodic (bool): Periodic boundary in x-direction
            y_periodic (bool): Periodic boundary in y-direction
        """
        pass
    
    @abstractmethod
    def get_domain_info(self) -> Dict:
        """
        Get complete domain information.
        
        Returns:
            dict: Domain details with keys:
                - 'xmin', 'xmax', 'ymin', 'ymax': Bounds (m)
                - 'dx', 'dy': Grid spacing (m)
                - 'nx', 'ny': Grid dimensions
                - 'domain_area': Total domain area (m²)
        """
        pass
    
    # ==================== OUTPUT AND VISUALIZATION ====================
    
    @abstractmethod
    def write_plotfile(self, filename: str) -> None:
        """
        Write AMReX-format plotfile for visualization.
        
        Parameters:
            filename (str): Output filename (without extension)
        """
        pass
    
    @abstractmethod
    def export_csv(self, filename: str, fields: Optional[List[str]] = None) -> None:
        """
        Export fire state to CSV for analysis.
        
        Parameters:
            filename (str): Output CSV filename
            fields (list, optional): Subset of fields to export.
                    Options: "phi", "ros", "intensity", "fuel_consumed", etc.
                    Default: all available fields
        """
        pass
    
    @abstractmethod
    def export_geotiff(self, filename: str, field: str = "ros",
                      georeference: Optional[Dict] = None) -> None:
        """
        Export field as GeoTIFF with optional georeferencing.
        
        Parameters:
            filename (str): Output GeoTIFF filename
            field (str): Field to export ("ros", "intensity", "phi", etc.)
            georeference (dict, optional): Georeferencing info (CRS, geotransform, etc.)
        """
        pass
    
    # ==================== DIAGNOSTIC AND ANALYSIS ====================
    
    @abstractmethod
    def compute_fire_perimeter(self) -> float:
        """
        Calculate active fire perimeter length.
        
        Returns:
            float: Perimeter length (m)
        """
        pass
    
    @abstractmethod
    def compute_burned_area_fraction(self) -> float:
        """
        Return fraction of domain currently burned.
        
        Returns:
            float: Burned fraction [0, 1]
        """
        pass
    
    @abstractmethod
    def compute_fire_statistics(self) -> Dict:
        """
        Compute comprehensive fire statistics.
        
        Returns:
            dict: Statistics with keys:
                - 'max_ros', 'mean_ros', 'std_ros': ROS statistics (m/min)
                - 'ros_percentiles': Dict with "10th", "25th", "50th", "75th", "90th" values
                - 'perimeter_length': Active fire front perimeter (m)
                - 'burned_area': Area burned (m²)
                - 'max_intensity', 'mean_intensity': Intensity statistics (kW/m)
        """
        pass
    
    @abstractmethod
    def compute_ros_sensitivity(self, parameter: str,
                               delta: float = 0.1) -> Dict:
        """
        Compute sensitivity of ROS to parameter variations.
        
        Parameters:
            parameter (str): Parameter to vary: "moisture", "wind_speed", "fuel_load", etc.
            delta (float): Perturbation fraction (default 0.1 = ±10%)
        
        Returns:
            dict: Sensitivity analysis results
        """
        pass
    
    # ==================== FINALIZATION ====================
    
    @abstractmethod
    def finalize(self) -> bool:
        """
        Clean up solver resources and close I/O.
        
        Returns:
            bool: True if finalization succeeded
        """
        pass


# =====================================================================
# REFERENCE IMPLEMENTATION NOTES
# =====================================================================

"""
IMPLEMENTATION CHECKLIST FOR wildfire_levelset:

1. Core Solver (__init__, finalize)
   - [ ] Parse input file with new parameters (spread_model, fuel data, terrain data)
   - [ ] Initialize domain and grid from input file
   - [ ] Create storage for all required arrays

2. Wind Coupling (update_wind_3d, get_surface_fluxes)
   - [ ] Extract 2D wind at flame height from 3D wind field
   - [ ] Handle wind speed/direction from components
   - [ ] Store wind for ROS calculations
   - [ ] Compute heat/intensity fields for feedback

3. ROS Calculations (compute_rothermel_ros, compute_richards_ros)
   - [ ] Implement full Rothermel 1972 algorithm with all 13 fuel models
   - [ ] Implement Richards 1990 algorithm with explicit components
   - [ ] Handle edge cases: zero wind, extreme slopes, high moisture
   - [ ] Validate against published test cases and field data

4. State Management (get_state, get_fuel_data, get_ros_components)
   - [ ] Maintain phi (level-set), ROS, and intensity arrays
   - [ ] Track fuel state: model, moisture, load
   - [ ] Decompose ROS into components for diagnostics

5. Data I/O (set_fuel_model_map, set_fuel_moisture_field, etc.)
   - [ ] Load/save from CSV, GeoTIFF, NetCDF
   - [ ] Validate grid alignment with wind solver
   - [ ] Support multiple data formats

6. Time Integration (step, advance_to_time)
   - [ ] Implement adaptive CFL-based timesteps
   - [ ] Use computed ROS for front advancement
   - [ ] Track convergence and iterations

7. Output (write_plotfile, export_csv, export_geotiff)
   - [ ] Write AMReX plotfiles with standard format
   - [ ] Export for analysis and visualization
   - [ ] Georeferencing for GIS integration

VALIDATION:
- [ ] Test against published benchmark cases
- [ ] Verify ROS values vs empirical observations
- [ ] Check energy conservation and flame heights
- [ ] Validate coupling with wind solver
- [ ] Regression test suite passes
"""

