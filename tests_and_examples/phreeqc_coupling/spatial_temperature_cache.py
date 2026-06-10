#!/usr/bin/env python3
"""
spatial_temperature_cache.py - Spatially-Varying Temperature Field Example

Demonstrates localized T(x,y,z) field generation for PHREEQC integration.
Uses scenario library caching to enable fast (<30 sec) temperature exports.

Workflow:
1. Generate or load pre-computed scenario library (100 scenarios)
2. For current weather, find nearest scenario
3. Interpolate T(z) profiles for each (x,y) location
4. Apply elevation corrections for topography
5. Export spatial temperature field in NetCDF or ASCII format
6. Generate PHREEQC boundary conditions for reactive transport columns

References:
    - Lapse rates: Businger et al. (1971), Stull (2011)
    - PHREEQC integration: Parkhurst & Appelo (2013)
"""

import numpy as np
from pathlib import Path
import logging
import sys

# Add src/python directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "python"))

from phreeqc_coupling.scenario_library import ScenarioLibrary, build_scenario_library
from phreeqc_coupling.spatial_temperature_cache import (
    export_spatial_temperature_with_caching,
    SpatialTemperatureCache
)

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def main():
    """Run spatial temperature field generation example."""
    
    print("=" * 70)
    print("Spatial Temperature Field Generation with Caching")
    print("=" * 70)
    
    # Step 1: Build or load scenario library
    print("\n[Step 1] Loading/generating scenario library...")
    
    scenario_file = Path('scenario_library.hdf5')
    
    if scenario_file.exists():
        logger.info(f"Loading pre-computed scenario library from {scenario_file}")
        lib = ScenarioLibrary(n_scenarios=100)
        try:
            lib.load_from_hdf5(str(scenario_file))
            logger.info(f"Loaded {len(lib.scenarios)} scenarios")
        except Exception as e:
            logger.warning(f"Could not load HDF5, generating fresh library: {e}")
            result = build_scenario_library(n_scenarios=100, output_file=str(scenario_file))
            lib = result['library']
    else:
        logger.info("Generating scenario library (first-time, ~1-2 hours with full parallelization)")
        result = build_scenario_library(n_scenarios=100, output_file=str(scenario_file))
        lib = result['library']
    
    print(f"  ✓ Scenario library: {len(lib.scenarios)} scenarios")
    print(f"  ✓ Wind speed range: {result['u_mag_range'][0]:.1f}-{result['u_mag_range'][1]:.1f} m/s")
    print(f"  ✓ Temperature range: {result['temperature_range'][0]:.1f}-{result['temperature_range'][1]:.1f} K")
    
    # Step 2: Define spatial grid
    print("\n[Step 2] Defining spatial grid...")
    
    # Create 2D horizontal grid (1000 m × 1000 m domain, 50 m spacing)
    x_coords = np.arange(0, 1000, 50)
    y_coords = np.arange(0, 1000, 50)
    
    # Height coordinates (logarithmic spacing, 1 m to 1000 m)
    z_coords = np.logspace(0, 3, 20)
    
    print(f"  ✓ Horizontal grid: {len(x_coords)}×{len(y_coords)} points (50 m spacing)")
    print(f"  ✓ Vertical grid: {len(z_coords)} levels (1-1000 m)")
    print(f"  ✓ Total grid points: {len(x_coords) * len(y_coords) * len(z_coords):,}")
    
    # Step 3: Create synthetic elevation field (optional)
    print("\n[Step 3] Creating elevation field...")
    
    # Gaussian hill at center of domain
    x_grid, y_grid = np.meshgrid(x_coords, y_coords)
    center_x, center_y = 500, 500
    elevation = 200 * np.exp(-((x_grid - center_x)**2 + (y_grid - center_y)**2) / (200**2))
    elevation = elevation.T  # Transpose to match grid orientation
    
    print(f"  ✓ Elevation range: {elevation.min():.1f}-{elevation.max():.1f} m")
    
    # Step 4: Define current weather conditions
    print("\n[Step 4] Setting current weather conditions...")
    
    current_weather = {
        'u_mag': 8.5,        # m/s
        'wind_direction': 270,  # degrees (west wind)
        'temperature': 293.15,  # K (20°C)
    }
    
    print(f"  ✓ Wind speed: {current_weather['u_mag']:.1f} m/s")
    print(f"  ✓ Wind direction: {current_weather['wind_direction']:.0f}°")
    print(f"  ✓ Temperature: {current_weather['temperature']-273.15:.1f}°C")
    
    # Step 5: Export spatial temperature field (with caching)
    print("\n[Step 5] Exporting spatial temperature field...")
    
    T_field = export_spatial_temperature_with_caching(
        lib,
        weather_u_mag=current_weather['u_mag'],
        weather_wind_dir=current_weather['wind_direction'],
        weather_temperature=current_weather['temperature'],
        x_coords=x_coords,
        y_coords=y_coords,
        z_coords=z_coords,
        elevation_data=elevation
    )
    
    print(f"  ✓ Temperature field exported")
    print(f"    - Grid shape: {T_field.T_field.shape}")
    print(f"    - T range: {T_field.T_field.min():.1f}-{T_field.T_field.max():.1f} K")
    print(f"    - T range: {T_field.T_field.min()-273.15:.1f}-{T_field.T_field.max()-273.15:.1f}°C")
    
    # Step 6: Extract profiles at specific locations
    print("\n[Step 6] Extracting temperature profiles for PHREEQC columns...")
    
    phreeqc_locations = [
        (250.0, 500.0, "Upwind location"),
        (500.0, 500.0, "Facility center"),
        (750.0, 500.0, "Downwind location"),
    ]
    
    cache = SpatialTemperatureCache(lib)
    
    for x_loc, y_loc, description in phreeqc_locations:
        bc = cache.export_phreeqc_boundary_conditions(T_field, x_loc, y_loc)
        
        print(f"\n  {description}: ({bc['x']:.1f}, {bc['y']:.1f})")
        print(f"    Scenario ID: {bc['scenario_id']}")
        print(f"    T(z=1m) = {bc['T_profile'][0]-273.15:.1f}°C")
        print(f"    T(z=100m) = {bc['T_profile'][10]-273.15:.1f}°C")
        print(f"    T(z=1000m) = {bc['T_profile'][-1]-273.15:.1f}°C")
    
    # Step 7: Export to file formats
    print("\n[Step 7] Exporting results to file formats...")
    
    output_dir = Path('./output_temperature')
    output_dir.mkdir(exist_ok=True)
    
    # NetCDF export
    try:
        netcdf_file = output_dir / 'spatial_temperature.nc'
        cache.export_to_netcdf(T_field, str(netcdf_file))
        print(f"  ✓ NetCDF export: {netcdf_file}")
    except Exception as e:
        logger.warning(f"NetCDF export failed (netCDF4 not available?): {e}")
    
    # ASCII export (sample locations)
    print(f"  ✓ ASCII profiles exported (sample: 10 locations)")
    ascii_files = cache.export_to_ascii(T_field, str(output_dir))[:10]
    
    # Step 8: Summary statistics
    print("\n[Step 8] Summary statistics...")
    
    print(f"\n  Temperature field statistics:")
    print(f"    - Global min: {T_field.T_field.min():.2f} K ({T_field.T_field.min()-273.15:.1f}°C)")
    print(f"    - Global max: {T_field.T_field.max():.2f} K ({T_field.T_field.max()-273.15:.1f}°C)")
    print(f"    - Mean: {T_field.T_field.mean():.2f} K ({T_field.T_field.mean()-273.15:.1f}°C)")
    print(f"    - Std dev: {T_field.T_field.std():.2f} K")
    
    # Analyze vertical structure
    for z_idx in [0, len(z_coords)//2, -1]:
        z = z_coords[z_idx]
        T_2d = T_field.T_field[:, :, z_idx]
        print(f"    - T at z={z:.1f}m: {T_2d.min():.1f}-{T_2d.max():.1f} K "
              f"(range {T_2d.max()-T_2d.min():.1f} K)")
    
    print("\n" + "=" * 70)
    print("✓ Spatial temperature field generation complete")
    print(f"  Output directory: {output_dir}")
    print("=" * 70)
    
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as e:
        logger.error(f"Fatal error: {e}", exc_info=True)
        sys.exit(1)
