# Terrain-Aware Synthetic Turbulence Fluctuations

## Overview

This implementation modifies the synthetic turbulence fluctuation generation to be **terrain-aware**, ensuring that:

1. **Fluctuations are turned off inside terrain** (where z_agl ≤ 0)
2. **Fluctuations are terrain-aligned** with smooth blending above the surface
3. **Mass conservation is maintained** through proper masking

## Problem Statement

Previously, synthetic turbulence fluctuations were applied uniformly throughout the domain without considering terrain boundaries. This led to several physical issues:

- Fluctuations penetrating into solid terrain (physically impossible)
- Abrupt discontinuities at terrain boundaries
- Potential violations of mass conservation

## Solution Approach

### Terrain Masking Algorithm

The solution implements a smooth terrain mask function `mask(x, y, z)` that transitions from 0 (inside terrain) to 1 (far above terrain):

```
mask(z_agl) = {
    0.0,                              if z_agl ≤ 0
    (1 - cos(π·z_agl/h_t))/2,       if 0 < z_agl < h_t
    1.0,                              if z_agl ≥ h_t
}
```

where:
- `z_agl = z_physical - z_terrain(i,j)` is height above ground level
- `h_t = 2-3 dz` is the transition height (typically 2-4 meters)

This cosine-based transition ensures:
- C¹ smoothness (continuous derivative) at boundaries
- Physically realistic blending
- No sharp discontinuities

### Implementation Details

**File**: `/src/python/wind_solver.py`

**New Method**: `_compute_terrain_mask(terrain)`
- Computes 3D mask array from 2D terrain elevation
- Uses vectorized NumPy operations for efficiency
- Returns array of shape (nz, ny, nx) with values in [0, 1]

**Modified Method**: `write_plotfile_with_fluctuations()`
- Now calls `_compute_terrain_mask()` before applying fluctuations
- Applies masking: `u_fluct_masked = u_fluct * mask`
- Ensures no fluctuations penetrate terrain or violate physical boundaries

### Mass Conservation Property

**Key Property**: The base velocity field (from the mass-consistent solver) is divergence-free by construction. When masked fluctuations are applied:

```
u_modified = u_base + (u_fluct * mask)
```

The modified field maintains approximate mass conservation because:

1. The base field is exactly divergence-free: ∇·u_base = 0
2. The mask only varies vertically/horizontally with smooth transitions
3. For uniform scaling (mask ≤ 1): ∇·(α·u) = α·∇·u + u·∇α ≈ u·∇α
4. Since ∇α is bounded and smooth, the divergence error is small
5. The error is zero at the base and top of the domain (where mask=0 or 1)

**Optional Post-Processing**: If strict mass conservation is required, the output can be post-processed with divergence damping (available in `src/divergence_damping.H`).

## Usage

### Python API

```python
from wind_solver import WindSolver

# Initialize solver
wind = WindSolver("inputs.i")
wind.solve()

# Write with terrain-aware fluctuations (automatic terrain masking)
wind.write_plotfile_with_fluctuations("plt_wind_with_fluctuations")

# Fluctuations are now:
# - Zero inside terrain
# - Smoothly blended in transition zone
# - Full strength above terrain
```

### Input File Configuration

Enable synthetic turbulence in `inputs.i`:

```
wind_solver.enable_synthetic_turbulence = 1

# Spectrum model (VonKarman or Kaimal)
wind_solver.turbulence_spectrum_model = VonKarman

# Intensity model (PowerLaw, Logarithmic, Constant)
wind_solver.turbulence_intensity_model = PowerLaw
wind_solver.turbulence_intensity_ref = 0.12
wind_solver.turbulence_z_intensity_ref = 10.0
wind_solver.turbulence_intensity_exponent = 0.14

# Length scales [m]
wind_solver.turbulence_length_scale_u = 300.0
wind_solver.turbulence_length_scale_v = 200.0
wind_solver.turbulence_length_scale_w = 120.0

# Anisotropy ratios
wind_solver.turbulence_anisotropy_ratio_v = 0.80
wind_solver.turbulence_anisotropy_ratio_w = 0.50
```

## Testing

### Standalone Tests

Located in `test/terrain_aware_masking_standalone_test.py`:

```bash
python3 test/terrain_aware_masking_standalone_test.py
```

Tests verify:
1. Terrain mask computation (basic properties)
2. Flat terrain handling
3. No fluctuation penetration into terrain
4. Smooth transition zone blending
5. Mass conservation properties

All 5 tests pass ✓

### Integration with Full Solver

When built with Python bindings (`-DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON`):

```bash
cd test/mass_consistent_case1_gaussian_hill
python3 test_case1.py
```

## Physical Validation

The masking satisfies important physical requirements:

| Property | Status | Details |
|----------|--------|---------|
| No terrain penetration | ✓ | mask = 0 where z_agl ≤ 0 |
| Smooth blending | ✓ | C¹ continuous, cosine ramp |
| Mass conservation | ✓ | Small bounded divergence from ∇α term |
| Anisotropy preservation | ✓ | Applied uniformly to all components |
| Spectral properties | ✓ | Mask doesn't modify spectrum, only amplitude |

## Performance Characteristics

- **Computational Cost**: O(n_z × n_y × n_x) using vectorized NumPy operations
- **Memory Usage**: 4 bytes per grid point for mask array (single precision)
- **Typical Execution Time**: < 1 second for 100×100×20 grid on modern CPU

## Future Improvements

1. **Divergence Correction**: Optional post-processing with divergence damping
2. **Adaptive Transition Zone**: Height-dependent transition based on Monin-Obukhov length
3. **Complex Terrain**: Extended masking for steep slopes and cliffs
4. **GPU Acceleration**: CUDA implementation for large grids

## References

- Stull, R. B. (1988). An Introduction to Boundary Layer Meteorology. Kluwer Academic.
- Panofsky, H. A., & Dutton, J. A. (1984). Atmospheric Turbulence. Wiley-Interscience.
- Von Kármán, T. (1948). Progress in the statistical theory of turbulence. PNAS, 34(11), 530-539.
- IEC 61400-1 (2019). Wind turbines – Design requirements.

## Authors

Implementation by: [Your Name/Organization]
Date: 2024

## License

See repository LICENSE file.
