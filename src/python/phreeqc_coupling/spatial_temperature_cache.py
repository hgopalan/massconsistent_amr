#!/usr/bin/env python3
"""
spatial_temperature_cache.py - Localized Temperature Field Caching

Provides fast export of spatially-varying temperature fields T(x,y,z) by
interpolating from pre-computed scenario library. Enables location-specific
temperature boundary conditions for PHREEQC reactive transport simulation.

For each PHREEQC column at location (x,y), the cache returns local T(z)
from the nearest pre-computed scenario, with optional local adjustments.

Target performance: <30 seconds to export complete spatial field.

References:
    - Lapse rate climatology: Businger et al. (1971)
    - Temperature interpolation: Stull (2011), Boundary Layer Meteorology
    - Reactive transport coupling: Parkhurst & Appelo (2013), PHREEQC Manual
"""

import numpy as np
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Callable
from dataclasses import dataclass
import logging
from scipy.interpolate import interp1d

logger = logging.getLogger(__name__)


@dataclass
class SpatialTemperatureField:
    """Container for spatially-varying temperature field.
    
    Attributes:
        x_coords (np.ndarray): x grid coordinates (m)
        y_coords (np.ndarray): y grid coordinates (m)
        z_coords (np.ndarray): z height coordinates (m)
        T_field (np.ndarray): Temperature field [nx, ny, nz] (K)
        T_profile (np.ndarray): Reference height profile [nz] (K)
        scenario_ids (np.ndarray): Source scenario ID at each (x,y) [nx, ny]
    """
    x_coords: np.ndarray
    y_coords: np.ndarray
    z_coords: np.ndarray
    T_field: np.ndarray
    T_profile: np.ndarray
    scenario_ids: np.ndarray


class SpatialTemperatureCache:
    """Fast spatial temperature field interpolation from scenario library.
    
    Given a scenario library, provides methods to:
    1. Find nearest scenario for current weather at each (x,y)
    2. Interpolate temperature profile from cached scenario
    3. Apply local adjustments (elevation, topographic shading, etc.)
    4. Return complete T(x,y,z) field for PHREEQC integration
    """
    
    def __init__(self, scenario_library):
        """Initialize cache with scenario library.
        
        Args:
            scenario_library: ScenarioLibrary instance (from scenario_library.py)
        """
        self.library = scenario_library
        self._interp_cache = {}  # Cache interpolators for frequently used scenarios
        
        logger.info("Initialized SpatialTemperatureCache")
    
    def export_spatial_field(
        self,
        x_coords: np.ndarray,
        y_coords: np.ndarray,
        z_coords: np.ndarray,
        weather_u_mag: float,
        weather_wind_dir: float,
        weather_temperature: float,
        elevation_data: Optional[np.ndarray] = None,
        elevation_correction: bool = True
    ) -> SpatialTemperatureField:
        """Export spatially-varying temperature field.
        
        Fast routine (<30 sec) to compute T(x,y,z) at all grid points:
        1. Find nearest scenario to current weather
        2. For each (x,y): interpolate T profile from cache
        3. Apply elevation correction if topography provided
        4. Return complete field and metadata
        
        Args:
            x_coords: X grid coordinates [m]
            y_coords: Y grid coordinates [m]
            z_coords: Height coordinates [m] (same for all x,y)
            weather_u_mag: Current wind speed (m/s)
            weather_wind_dir: Current wind direction (degrees)
            weather_temperature: Current temperature (K)
            elevation_data: Optional elevation map [nx, ny] (m)
            elevation_correction: Apply elevation-based temperature correction (default True)
        
        Returns:
            SpatialTemperatureField with T(x,y,z) and metadata
        """
        nx, ny = len(x_coords), len(y_coords)
        nz = len(z_coords)
        
        logger.info(f"Exporting spatial T field: {nx}×{ny}×{nz} grid...")
        
        # Find nearest scenario (global, independent of location)
        scenario = self.library.nearest_scenario(
            weather_u_mag, weather_wind_dir, weather_temperature
        )
        
        logger.debug(f"Using scenario {scenario.weather_id}: u_mag={scenario.u_mag_ref:.1f} m/s")
        
        # Get or create interpolator for this scenario
        interp_func = self._get_temperature_interpolator(scenario)
        
        # Allocate output field
        T_field = np.zeros((nx, ny, nz))
        scenario_ids = np.full((nx, ny), scenario.weather_id, dtype=int)
        
        # Interpolate T at each (x,y,z) point
        for i in range(nx):
            for j in range(ny):
                # Base temperature from scenario interpolation
                T_local = interp_func(z_coords)
                
                # Apply elevation correction if available
                if elevation_data is not None and elevation_correction:
                    elev_xy = elevation_data[i, j]
                    T_local = self._apply_elevation_correction(
                        T_local, z_coords, elev_xy, scenario.stability_class
                    )
                
                T_field[i, j, :] = T_local
        
        # Compute reference profile (average over domain)
        T_profile = np.mean(T_field, axis=(0, 1))
        
        logger.info(f"Spatial T field exported: min={T_field.min():.1f}K, "
                   f"max={T_field.max():.1f}K")
        
        return SpatialTemperatureField(
            x_coords=x_coords,
            y_coords=y_coords,
            z_coords=z_coords,
            T_field=T_field,
            T_profile=T_profile,
            scenario_ids=scenario_ids
        )
    
    def _get_temperature_interpolator(self, scenario) -> Callable:
        """Get or create cached interpolator for scenario temperature profile.
        
        Args:
            scenario: WeatherScenario object
        
        Returns:
            Callable interpolation function: f(z) → T(z)
        """
        scenario_id = scenario.weather_id
        
        if scenario_id not in self._interp_cache:
            # Create interpolator (linear, extrapolate as constant)
            interp = interp1d(
                scenario.heights, scenario.T_profile,
                kind='linear', bounds_error=False,
                fill_value=(scenario.T_profile[0], scenario.T_profile[-1])
            )
            self._interp_cache[scenario_id] = interp
        
        return self._interp_cache[scenario_id]
    
    @staticmethod
    def _apply_elevation_correction(
        T_local: np.ndarray,
        z_coords: np.ndarray,
        elevation: float,
        stability_class: str,
        reference_elevation: float = 0.0
    ) -> np.ndarray:
        """Apply elevation-based temperature lapse rate correction.
        
        For terrain with elevation variation, adjust temperatures based on
        local elevation relative to reference. Uses dry or moist adiabatic
        lapse rate depending on stability.
        
        Args:
            T_local: Temperature profile at reference elevation [nz]
            z_coords: Height coordinates above reference elevation [nz]
            elevation: Local elevation above reference [m]
            stability_class: PGT stability class (A-F) to determine lapse rate
            reference_elevation: Reference elevation (default 0)
        
        Returns:
            Adjusted temperature profile [nz]
        """
        # Lapse rates by stability (K/km, converted to K/m)
        lapse_rates = {
            'A': 0.005,    # Very unstable: weak lapse
            'B': 0.007,
            'C': 0.0098,   # Neutral
            'D': 0.0098,
            'E': 0.015,    # Stable: strong inversion
            'F': 0.020
        }
        lapse = lapse_rates.get(stability_class, 0.0098)
        
        # Elevation difference from reference
        delta_elev = elevation - reference_elevation
        
        # Temperature adjustment: ΔT = -lapse × Δelev
        T_adjusted = T_local - lapse * delta_elev
        
        return T_adjusted
    
    def export_phreeqc_boundary_conditions(
        self,
        spatial_field: SpatialTemperatureField,
        x_phreeqc: float,
        y_phreeqc: float
    ) -> Dict[str, any]:
        """Extract temperature profile for PHREEQC 1D column at specific location.
        
        Given a spatial temperature field and (x,y) location, return the
        local temperature profile T(z) formatted for PHREEQC input.
        
        Args:
            spatial_field: SpatialTemperatureField from export_spatial_field()
            x_phreeqc: x coordinate of PHREEQC column (m)
            y_phreeqc: y coordinate of PHREEQC column (m)
        
        Returns:
            Dictionary with keys:
            - 'z_coords': height coordinates [m]
            - 'T_profile': temperature profile [K]
            - 'x': column x coordinate
            - 'y': column y coordinate
            - 'scenario_id': source scenario ID
        """
        # Find closest grid point
        i = np.argmin(np.abs(spatial_field.x_coords - x_phreeqc))
        j = np.argmin(np.abs(spatial_field.y_coords - y_phreeqc))
        
        return {
            'z_coords': spatial_field.z_coords,
            'T_profile': spatial_field.T_field[i, j, :],
            'x': spatial_field.x_coords[i],
            'y': spatial_field.y_coords[j],
            'scenario_id': int(spatial_field.scenario_ids[i, j])
        }
    
    def export_to_netcdf(
        self,
        spatial_field: SpatialTemperatureField,
        output_file: str
    ) -> None:
        """Export spatial temperature field to NetCDF format.
        
        Args:
            spatial_field: SpatialTemperatureField to export
            output_file: Output NetCDF file path
        """
        try:
            import netCDF4
        except ImportError:
            logger.warning("netCDF4 not available, skipping NetCDF export")
            return
        
        with netCDF4.Dataset(output_file, 'w', format='NETCDF4') as ds:
            # Create dimensions
            ds.createDimension('x', len(spatial_field.x_coords))
            ds.createDimension('y', len(spatial_field.y_coords))
            ds.createDimension('z', len(spatial_field.z_coords))
            
            # Create coordinate variables
            x_var = ds.createVariable('x', 'f4', ('x',))
            y_var = ds.createVariable('y', 'f4', ('y',))
            z_var = ds.createVariable('z', 'f4', ('z',))
            
            x_var[:] = spatial_field.x_coords
            y_var[:] = spatial_field.y_coords
            z_var[:] = spatial_field.z_coords
            
            x_var.units = 'm'
            y_var.units = 'm'
            z_var.units = 'm'
            
            # Create temperature variable
            T_var = ds.createVariable('T', 'f4', ('x', 'y', 'z'))
            T_var[:] = spatial_field.T_field
            T_var.units = 'K'
            T_var.long_name = 'Temperature'
            
            # Create scenario ID variable
            sid_var = ds.createVariable('scenario_id', 'i4', ('x', 'y'))
            sid_var[:] = spatial_field.scenario_ids
            
            # Global attributes
            ds.description = 'Spatially-varying temperature field from scenario library'
            ds.source = 'SpatialTemperatureCache'
        
        logger.info(f"Exported spatial temperature field to {output_file}")
    
    def export_to_ascii(
        self,
        spatial_field: SpatialTemperatureField,
        output_dir: str = './'
    ) -> List[str]:
        """Export spatial temperature field as ASCII files.
        
        Creates one file per (x,y) point with format:
        z (m) | T (K)
        
        Args:
            spatial_field: SpatialTemperatureField to export
            output_dir: Output directory for ASCII files
        
        Returns:
            List of created file paths
        """
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        
        created_files = []
        
        for i, x in enumerate(spatial_field.x_coords):
            for j, y in enumerate(spatial_field.y_coords):
                filename = output_path / f"T_profile_x{i:03d}_y{j:03d}.txt"
                
                with open(filename, 'w') as f:
                    f.write("# Temperature profile from spatial cache\n")
                    f.write(f"# Location: x={x:.2f}m, y={y:.2f}m\n")
                    f.write(f"# Scenario ID: {spatial_field.scenario_ids[i,j]}\n")
                    f.write("z(m)\tT(K)\n")
                    
                    for k, z in enumerate(spatial_field.z_coords):
                        T = spatial_field.T_field[i, j, k]
                        f.write(f"{z:.2f}\t{T:.2f}\n")
                
                created_files.append(str(filename))
        
        logger.info(f"Exported {len(created_files)} ASCII temperature profiles")
        
        return created_files


def export_spatial_temperature_with_caching(
    scenario_library,
    weather_u_mag: float,
    weather_wind_dir: float,
    weather_temperature: float,
    x_coords: np.ndarray,
    y_coords: np.ndarray,
    z_coords: np.ndarray,
    elevation_data: Optional[np.ndarray] = None
) -> SpatialTemperatureField:
    """Convenience function to export spatial temperature field with caching.
    
    One-line interface to the complete caching workflow.
    
    Args:
        scenario_library: ScenarioLibrary instance
        weather_u_mag: Current wind speed (m/s)
        weather_wind_dir: Current wind direction (degrees)
        weather_temperature: Current temperature (K)
        x_coords: X grid coordinates (m)
        y_coords: Y grid coordinates (m)
        z_coords: Height coordinates (m)
        elevation_data: Optional elevation map (m)
    
    Returns:
        SpatialTemperatureField with cached T(x,y,z)
    
    Example:
        >>> from scenario_library import ScenarioLibrary
        >>> lib = ScenarioLibrary(n_scenarios=100)
        >>> lib.load_from_hdf5('scenario_library.hdf5')
        >>> T_field = export_spatial_temperature_with_caching(
        ...     lib, u_mag=8.5, wind_dir=270, temperature=293.15,
        ...     x_coords=np.arange(0, 1000, 100),
        ...     y_coords=np.arange(0, 1000, 100),
        ...     z_coords=np.logspace(1, 3, 20)
        ... )
        >>> print(f"T range: {T_field.T_field.min():.1f} - {T_field.T_field.max():.1f} K")
    """
    cache = SpatialTemperatureCache(scenario_library)
    
    return cache.export_spatial_field(
        x_coords=x_coords,
        y_coords=y_coords,
        z_coords=z_coords,
        weather_u_mag=weather_u_mag,
        weather_wind_dir=weather_wind_dir,
        weather_temperature=weather_temperature,
        elevation_data=elevation_data,
        elevation_correction=True
    )


if __name__ == '__main__':
    # Example usage
    logging.basicConfig(level=logging.INFO)
    
    from scenario_library import ScenarioLibrary
    
    # Build or load scenario library
    lib = ScenarioLibrary(n_scenarios=50)
    lib.generate_scenarios()
    
    # Create test grid
    x = np.arange(0, 1000, 100)
    y = np.arange(0, 1000, 100)
    z = np.logspace(1, 3, 20)
    
    # Export spatial field
    T_field = export_spatial_temperature_with_caching(
        lib,
        weather_u_mag=8.5,
        weather_wind_dir=270,
        weather_temperature=293.15,
        x_coords=x,
        y_coords=y,
        z_coords=z
    )
    
    print(f"Exported spatial field: {T_field.T_field.shape}")
    print(f"Temperature range: {T_field.T_field.min():.1f} - {T_field.T_field.max():.1f} K")
