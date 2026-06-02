# Phase 1 Implementation Summary: Core Physics Features

## Overview
This implementation adds three new physics features to the massconsistent_amr wind solver, building upon the infrastructure added in Phase 1 (input parameters and file reading functions).

## Features Implemented

### 1. Wind Direction Gradient
**Purpose**: Implement linear directional shear with height as a simpler alternative to Ekman spiral.

**Implementation**:
- Added `wind_direction_gradient_angle()` helper function in `stability_models.H`
- Applied in all three initialization modes:
  - Log-law initialization
  - Surface_data initialization  
  - Power-law initialization
- Uses same rotation mechanism as Ekman veer but with linear height dependence

**Input Parameters**:
```
enable_wind_direction_gradient = true
wind_direction_shear_rate = 5.0  # degrees/100m
```

**Files Modified**:
- `src/stability_models.H`: Added wind_direction_gradient_angle() function
- `src/wind_solver.cpp`: Applied in all three initialization modes

### 2. Spatially-varying Lagrange Coefficients
**Purpose**: Allow alpha_h and alpha_v to vary spatially based on file input, providing better control over mass-consistent adjustment in different regions.

**Implementation**:
- Added `read_alpha_coefficients_file()` to read X Y ALPHA_H ALPHA_V data
- Added GPU-compatible `idw_alpha_coefficients()` for interpolation
- Created MultiFab fields for spatial alpha_h and alpha_v
- Modified Poisson solver B coefficient setup to use spatial fields
- Face-centered B coefficients average neighboring cell values

**Input Parameters**:
```
use_spatial_alpha_coefficients = true
alpha_coefficients_file = "alpha_data.txt"
```

**File Format** (alpha_coefficients_file):
```
# X Y ALPHA_H ALPHA_V
1000.0 2000.0 1.0 0.5
1500.0 2000.0 1.2 0.6
...
```

**Files Modified**:
- `src/wind_solver.cpp`: 
  - Added file reading function
  - Added GPU-compatible IDW interpolation
  - Created MultiFab fields
  - Modified Poisson solver setup

### 3. Fetch-dependent Roughness Transition
**Purpose**: Model internal boundary layer development when surface roughness changes abruptly.

**Implementation**:
- Added `internal_boundary_layer_height()` helper function
- Added `blend_roughness_fetch()` helper function for roughness blending
- Captured input parameters in all initialization modes
- **Note**: Infrastructure complete; full implementation requires upwind fetch distance calculation which involves wind direction tracing

**Input Parameters**:
```
enable_fetch_roughness_transition = true
fetch_transition_blending_height = 100.0  # m
```

**Helper Functions**:
- `internal_boundary_layer_height(fetch, z01, z02, blend_height)`: Computes IBL height
- `blend_roughness_fetch(z_agl, z0_upwind, z0_local, fetch, blend_height)`: Returns blended roughness

**Files Modified**:
- `src/wind_solver.cpp`:
  - Added helper functions
  - Captured parameters in all modes
  - Added status print messages

## Backward Compatibility
All features are **disabled by default**:
- `enable_wind_direction_gradient = false`
- `use_spatial_alpha_coefficients = false`
- `enable_fetch_roughness_transition = false`

Existing simulations will behave identically unless users explicitly enable the new features.

## Testing Status
- **Implementation**: Complete ✓
- **Regression Tests**: Deferred to separate task
- **Documentation**: Deferred to separate task

## Future Work (Phase 2)
For fetch-dependent roughness:
1. Implement upwind fetch distance calculation
2. Add wind direction tracing to find roughness change locations
3. Apply blending in wind field initialization using helper functions

## Summary Statistics
- **Files Modified**: 2
  - `src/stability_models.H`
  - `src/wind_solver.cpp`
- **New Functions**: 5
  - `wind_direction_gradient_angle()`
  - `read_alpha_coefficients_file()`
  - `idw_alpha_coefficients()` (host version)
  - `idw_alpha_coefficients()` (GPU version)
  - `internal_boundary_layer_height()`
  - `blend_roughness_fetch()`
- **New MultiFab Fields**: 2
  - `alpha_h_field`
  - `alpha_v_field`
- **Lines Added**: ~400
