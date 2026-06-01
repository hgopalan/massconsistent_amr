# Wall Functions Implementation

## Overview

This implementation adds support for log-law wall functions at solid boundaries (terrain, flat surfaces, and building walls) as an alternative to traditional no-slip (zero velocity) boundary conditions.

## Key Features

- **Switchable boundary conditions**: Users can choose between no-slip and log-law wall functions via input parameters
- **Backward compatible**: Defaults to no-slip boundary conditions (existing behavior)
- **GPU-compatible**: All wall function kernels work on both CPU and GPU
- **Multiple surface types**: Supports terrain, flat surfaces, and building walls
- **Blending function**: Smooth transition between wall function region and outer flow

## Input Parameters

### Master Control
```
enable_wall_functions = false  # Master enable/disable (default: false for backward compatibility)
```

### Surface-Specific Controls
```
enable_terrain_wall_function = false       # Apply wall function at terrain boundaries
enable_flat_surface_wall_function = false  # Apply wall function at flat surfaces
enable_building_wall_function = false      # Apply wall function at building walls
```

### Wall Function Parameters
```
wall_function_z0_building = 0.001         # Building wall roughness [m]
wall_function_z0_flat = 0.01              # Flat surface roughness [m]
wall_function_blend_height = 2.0          # Blending layer height [cells]
wall_function_max_distance = 3.0          # Max distance for wall function [cells]
wall_function_min_wall_distance = 0.1     # Minimum distance from wall [m]
```

### Flat Surface Mode
```
wall_function_enable_flat_surface = false      # Enable flat surface mode
wall_function_flat_surface_elevation = 0.0     # Elevation of flat surface [m]
```

## Usage Examples

### Example 1: Enable Wall Functions for Terrain
```
# Enable wall functions with terrain boundary treatment
enable_wall_functions = true
enable_terrain_wall_function = true

# Wind parameters
init_mode = loglaw
U_ref = 10.0
z_ref = 10.0
z0 = 0.1
```

### Example 2: Compare No-Slip vs Wall Function
```
# Run 1: Traditional no-slip (default)
enable_wall_functions = false

# Run 2: Log-law wall function
enable_wall_functions = true
enable_terrain_wall_function = true
wall_function_blend_height = 2.0
```

## Implementation Details

### File Structure
- `src/wall_functions.H`: Core wall function models and GPU kernels
- `src/wind_solver.cpp`: Integration into wind field initialization

### Wall Function Formulation

For flat horizontal surfaces, the log-law wall function is:

```
u_parallel = (u*/κ) × ln((z + z0) / z0)
```

Where:
- `u*` is the friction velocity
- `κ = 0.41` is the von Karman constant
- `z` is the height above the surface
- `z0` is the surface roughness length

### Blending Function

A smooth Hermite interpolation blends the wall function with the outer flow:

```
blend(d) = t² × (3 - 2t)  where t = (d - d_start) / (d_end - d_start)
```

This ensures smooth transition between near-wall and outer regions.

## Test Cases

### Test 1: Flat Surface Wall Function
Location: `regtest/wall_function_flat/`

Simple flat terrain case to validate wall function implementation.

```bash
cd regtest/wall_function_flat
../../build/wind_solver inputs.i
```

### Test 2: Comparison (No-Slip vs Wall Function)
Location: `regtest/wall_function_comparison/`

Demonstrates difference between no-slip and wall function boundary conditions.

## Expected Behavior

### With Wall Functions Enabled
- Near-wall velocities follow log-law profile
- Smooth transition to outer flow
- More realistic near-surface wind profiles
- Better representation of wall shear stress

### With Wall Functions Disabled (Default)
- Traditional no-slip boundary conditions
- Zero velocity at terrain/building surfaces
- Backward compatible with existing simulations

## Physics Background

Wall functions are appropriate for:
- High Reynolds number turbulent flows
- Cases where grid resolution cannot resolve boundary layer
- Typical atmospheric boundary layer simulations

Wall functions provide:
- More realistic velocity profiles near walls
- Better estimates of wall shear stress
- Reduced sensitivity to near-wall grid resolution

## Future Enhancements

Potential improvements for future versions:
1. Non-horizontal terrain normal vectors (full 3D terrain following)
2. Building wall functions with face detection
3. Integration with wake models
4. Stability corrections in wall function
5. Adaptive activation based on grid resolution
6. Wall shear stress diagnostics output

## References

- Launder, B.E., & Spalding, D.B. (1974). The numerical computation of turbulent flows
- Pope, S.B. (2000). Turbulent Flows. Cambridge University Press
- Blocken, B., et al. (2007). CFD simulation of the atmospheric boundary layer: wall function problems
