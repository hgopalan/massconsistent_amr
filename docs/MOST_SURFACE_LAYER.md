# Monin-Obukhov Similarity Theory (MOST) Surface Layer Boundary Conditions

## Overview

The MOST surface layer boundary conditions apply Monin-Obukhov Similarity Theory to correct wind and temperature profiles at the first cell above terrain. This feature supports terrain-aware, spatially-varying surface parameters including:

- **Obukhov Length (L)**: Characterizes atmospheric stability (-∞ < L < +∞)
  - L > 0: Stable conditions (suppresses mixing)
  - L = ∞: Neutral conditions (no buoyancy effects)
  - L < 0: Unstable conditions (enhances mixing)

- **Roughness Length (z0)**: Surface roughness in meters (0.001 - 1.0 m typical range)

- **Sensible Heat Flux**: Surface heating in W/m² (optional, for temperature correction)

## Physics

The MOST profiles are based on the Businger-Dyer formulation:

```
u(z) = (u*/κ) * [ln((z+z0)/z0) - ψ_m(z/L) + ψ_m(z0/L)]
T(z) = T_ref + (θ*/κ) * [ln(z/z0) - ψ_h(z/L)]
```

Where:
- **u***: Friction velocity [m/s]
- **κ**: von Kármán constant (0.41)
- **ψ_m**: Stability function for momentum
- **ψ_h**: Stability function for heat
- **θ***: Temperature scale [K]

## Configuration Parameters

### Enable MOST Surface Layer

```
wind_solver.enable_most_surface_bc = true   # Enable MOST surface BC
```

### Atmospheric Stability

Specify via Obukhov length:

```
wind_solver.stability_length = 1000.0       # L = 1000 m (nearly neutral)
                                            # L > 0: stable
                                            # L < 0: unstable
```

Or from Pasquill-Gifford stability class:

```
wind_solver.enable_pg_stability = true
wind_solver.solar_radiation = 400.0         # W/m²
wind_solver.is_nighttime = false
wind_solver.cloud_cover = 0.5               # Fraction (0-1)
```

### Spatial Variability (Future)

File-based spatial maps (implementation pending):

```
wind_solver.L_obukhov_file = "L_obukhov_2d.csv"           # 2D Obukhov length map
wind_solver.sensible_heat_flux_file = "shf_2d.csv"        # 2D heat flux map
```

### Temperature Correction (Optional)

```
wind_solver.enable_most_temp_correction = true            # Apply temperature correction
wind_solver.enable_3d_scalars = true                       # Requires 3D scalar transport
```

## Stability Functions

### Businger-Dyer (Default)

Used for both stable and unstable conditions. Recommended for most applications.

```
psi_m(ζ) = -5.0 * ζ                    [stable, ζ > 0]
psi_m(ζ) = 2 ln((1+x)/2) + ...          [unstable, ζ < 0]
           where x = (1 - 16ζ)^0.25
```

### Holtslag-De Bruin (Alternative)

Better performance in very stable conditions (polar regions, nocturnal).

```
psi_m(ζ) = -(a*ζ + b(ζ - c/d)*exp(-d*ζ) + bc/d)
           where a=1.0, b=0.667, c=5.0, d=0.35
```

Enable via:

```
wind_solver.use_holtslag_stability = true
```

## Data Structures (C++)

### Device Arrays

```cpp
// In wind_solver_app.H:
amrex::Gpu::DeviceVector<amrex::Real> d_L_obukhov;           // [i + j*nx]
amrex::Gpu::DeviceVector<amrex::Real> d_sensible_heat_flux;  // [i + j*nx]
amrex::Gpu::DeviceVector<amrex::Real> d_ustar_field;         // [i + j*nx]
```

### Core Functions

Located in `src/surface_boundary_conditions.H`:

- `apply_most_wind_correction_at_first_cell()`: Wind profile correction
- `apply_most_temperature_correction_at_first_cell()`: Temperature profile correction
- `compute_friction_velocity_from_profile()`: Diagnostic u* calculation

## Example Usage

### Stable Atmosphere (Nighttime)

```
wind_solver.enable_most_surface_bc = true
wind_solver.stability_length = 500.0        # Stable (L > 0)
wind_solver.z0 = 0.1                         # Grass/short vegetation
```

### Unstable Atmosphere (Strong Heating)

```
wind_solver.enable_most_surface_bc = true
wind_solver.stability_length = -200.0        # Unstable (L < 0)
wind_solver.enable_most_temp_correction = true
```

### Neutral Atmosphere (Reference)

```
wind_solver.enable_most_surface_bc = true
wind_solver.stability_length = 10000.0       # Approaches neutral
```

## Implementation Details

1. **Application Timing**: MOST BC is applied AFTER wind profile initialization but BEFORE mass-consistent solver (Poisson solve)

2. **First Cell Only**: Corrections are applied at the first cell above terrain (z_agl > 0)

3. **Height Calculation**: Heights are computed as `z_agl = z_cell_center - terrain_elevation`

4. **GPU Support**: All kernels use `AMREX_GPU_DEVICE` for GPU acceleration

5. **No Breaking Changes**: Feature is optional; existing simulations run unchanged

## Performance Notes

- Minimal overhead: ~5-10% additional per wind field initialization
- GPU-accelerated: Fully compatible with CUDA/HIP backends
- Memory: ~3 extra 2D arrays when enabled (nx × ny real numbers each)

## Limitations & Future Work

- File reading for spatial L_obukhov and sensible heat flux maps not yet implemented
- Temperature correction requires 3D scalar transport to be meaningful
- Currently uses scalar friction velocity (could be spatially-varying)
- No adaptive stabilityfunction selection based on location

## References

- Businger, J. A., et al. (1971). Flux-Profile Relationships in the Atmospheric Surface Layer. J. Atmos. Sci., 28(2), 181-189.
- Paulson, C. A. (1970). The Mathematical Representation of Wind Speed and Temperature Profiles in the Unstable Atmospheric Surface Layer. J. Appl. Meteor., 9(6), 857-861.
- Monin, A. S., & Obukhov, A. M. (1954). Basic laws of turbulent mixing in the ground layer of the atmosphere. Tr. Akad. Nauk SSSR Geofiz. Inst., 24, 163-187.
- Holtslag, A. A. M., & De Bruin, H. A. R. (1988). Applied Modeling of the Nighttime Surface Energy Balance over Land. J. Appl. Meteor., 27, 689-704.

## Contact & Issues

For questions or issues with MOST surface layer BC, refer to the main README.md or contact the massconsistent_amr development team.
