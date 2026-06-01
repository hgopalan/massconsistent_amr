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

### Stability Correction Parameters
```
wall_function_enable_stability = false        # Enable Monin-Obukhov stability corrections
wall_function_stability_length = 1.0e10       # Obukhov length L [m] (>0 stable, <0 unstable)
```

### Adaptive Activation Parameters
```
wall_function_enable_adaptive = false         # Enable automatic activation based on grid resolution
wall_function_adaptive_threshold = 30.0       # Maximum dz/z0 ratio for activation
wall_function_adaptive_min_cells = 3.0        # Minimum cells in log layer for activation
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

## Advanced Features

### Stability Corrections

**Physical Motivation**: In real atmospheric boundary layers, thermal stratification affects the vertical wind profile. The neutral log-law is only valid when buoyancy effects are negligible. When the atmosphere is stable (nighttime cooling) or unstable (daytime heating), the wind profile deviates from the neutral log-law.

**Implementation**: The wall functions now support Monin-Obukhov similarity theory corrections via the Businger-Dyer stability functions. These modify the log-law based on the atmospheric stability parameter ζ = z/L, where L is the Obukhov length.

**Stability Regimes**:
- **L > 0 (stable)**: Suppressed turbulence, enhanced wind shear. Typical of nighttime boundary layers with surface cooling.
- **L < 0 (unstable)**: Enhanced turbulence, reduced wind shear. Typical of daytime boundary layers with surface heating.
- **|L| → ∞ (neutral)**: Standard log-law (no stability effects).

**Modified Log-Law**:
```
u(z) = (u*/κ) × [ln((z+z0)/z0) - ψ_m(z/L) + ψ_m(z0/L)]
```

where ψ_m is the stability function:
- **Stable (L > 0)**: ψ_m = -5ζ (linear suppression)
- **Unstable (L < 0)**: ψ_m follows Businger et al. (1971) formulation
- **Neutral**: ψ_m = 0 (reduces to standard log-law)

**Enabling Stability Corrections**:
```
enable_wall_functions = true
enable_terrain_wall_function = true

# Enable stability corrections
wall_function_enable_stability = true
wall_function_stability_length = 200.0  # Stable boundary layer (L > 0)

# Wind and roughness parameters
U_ref = 10.0
z_ref = 10.0
z0 = 0.1
```

**Example Values**:
- **Strongly unstable (daytime)**: L = -50 m
- **Weakly unstable**: L = -200 m
- **Neutral**: L > 10,000 m (or omit parameter)
- **Weakly stable**: L = 200 m
- **Strongly stable (nighttime)**: L = 50 m

**Physical Effects**:
- **Stable conditions** → steeper velocity gradients near surface, reduced mixing
- **Unstable conditions** → gentler velocity gradients, enhanced mixing
- The friction velocity u* is also affected by stability through the modified profile

**References**:
- Businger, J.A., et al. (1971). Flux-profile relationships in the atmospheric surface layer. *Journal of Atmospheric Sciences*, 28(2), 181-189.
- Dyer, A.J. (1974). A review of flux-profile relationships. *Boundary-Layer Meteorology*, 7(3), 363-372.

### Adaptive Activation

**Motivation**: Wall functions are only valid when the first grid cell is within the logarithmic layer but not too close to the wall. If the grid is too coarse (first cell above the log layer) or too fine (first cell inside the roughness sublayer), wall functions become inaccurate.

**Implementation**: The solver can now automatically enable or disable wall functions based on local grid resolution. This ensures wall functions are only applied where they are physically appropriate.

**Activation Criteria**:
1. **Above roughness**: First cell height dz > z0
2. **Within log layer**: dz/z0 < threshold (default: 30)
3. **Sufficient resolution**: At least min_cells cells within log layer region (default: 3)

**Grid Resolution Guidelines**:
- **Good**: 5 < dz/z0 < 30 (wall function region)
- **Too fine**: dz/z0 < 5 (resolve boundary layer directly, no wall function needed)
- **Too coarse**: dz/z0 > 30 (wall function inaccurate)

**Enabling Adaptive Activation**:
```
enable_wall_functions = true
enable_terrain_wall_function = true

# Enable adaptive activation
wall_function_enable_adaptive = true
wall_function_adaptive_threshold = 30.0     # Max dz/z0 ratio
wall_function_adaptive_min_cells = 3.0      # Min cells in log layer

# If grid is too coarse or too fine, wall functions automatically disabled
z0 = 0.1                                     # Surface roughness [m]
dz = 2.0                                     # Grid spacing [m]
# dz/z0 = 20 → wall function ACTIVE (within range)
```

**Example Scenarios**:
1. **Fine grid (dz = 0.5 m, z0 = 0.1 m)**:
   - dz/z0 = 5 → **Borderline** (wall function may activate)
   - If dz < z0, automatically disabled (grid resolves roughness elements)

2. **Coarse grid (dz = 5.0 m, z0 = 0.1 m)**:
   - dz/z0 = 50 → **Too coarse** (automatically disabled)
   - Need finer grid or use no-slip boundary condition

3. **Appropriate grid (dz = 2.0 m, z0 = 0.1 m)**:
   - dz/z0 = 20 → **Good** (wall function active)
   - First cell at 2 m height with z0 = 0.1 m is in log layer

**Physical Interpretation**:
- **dz << z0**: Grid is inside roughness sublayer (canopy, building roughness)
- **z0 < dz < 0.1δ**: Log layer region (wall functions appropriate)
- **dz > 0.1δ**: Outer boundary layer (wall functions less accurate)

where δ is the boundary layer height (typically ~1000 m for atmospheric BL).

**Benefits**:
- Automatic adaptation to varying roughness (forest → urban → water transitions)
- Prevents wall function errors in coarse simulations
- Ensures physical consistency across resolution changes

**Combining Features**:

Both features can be used together for maximum realism:
```
# Stable atmospheric conditions with adaptive activation
enable_wall_functions = true
enable_terrain_wall_function = true

# Stability correction for stable nighttime BL
wall_function_enable_stability = true
wall_function_stability_length = 100.0      # Stable (L > 0)

# Adaptive activation for varying roughness
wall_function_enable_adaptive = true
wall_function_adaptive_threshold = 30.0

# Surface parameters
z0 = 0.1                                     # Can vary spatially
U_ref = 8.0
z_ref = 10.0
```

## Future Enhancements

Potential improvements for future versions:
1. Non-horizontal terrain normal vectors (full 3D terrain following)
2. Building wall functions with face detection
3. Integration with wake models
4. Wall shear stress diagnostics output

## References

- Launder, B.E., & Spalding, D.B. (1974). The numerical computation of turbulent flows
- Pope, S.B. (2000). Turbulent Flows. Cambridge University Press
- Blocken, B., et al. (2007). CFD simulation of the atmospheric boundary layer: wall function problems
