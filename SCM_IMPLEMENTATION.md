# Single Column Model (SCM) Implementation

## Overview

The Single Column Model (SCM) is a new initialization mode for the mass-consistent wind solver that enables direct specification of wind speed at a reference height (e.g., meteorological mast height). Instead of using log-law assumptions, the SCM performs a time-dependent 1D simulation to determine the geostrophic wind required to produce the specified wind speed at the reference height.

## Motivation

Traditional initialization using log-law is based on strong assumptions about atmospheric stability and may not be accurate in complex terrain. The SCM approach is more physics-based and handles:
- Coriolis forcing
- Turbulent diffusion with variable eddy viscosity
- Temperature profiles with lapse rates
- Turbulent kinetic energy evolution
- Stratification effects

## Reference Implementation

The implementation is based on the `hrrr_1dsolver_terrain.py` solver from the [onedterrainsolver](https://github.com/hgopalan/onedterrainsolver) repository, which is a physics-based 1D column model that:
1. Solves wind momentum equations with Coriolis and turbulent diffusion
2. Evolves temperature with diffusive mixing
3. Solves a 1-equation turbulent kinetic energy (TKE) model
4. Computes eddy viscosity from TKE and mixing length
5. Uses Monin-Obukhov similarity theory for surface layer

## Algorithm

### Step 1: Geostrophic Wind Recursion

The algorithm iteratively finds geostrophic wind components (Ug, Vg) such that:
- Initial guess: Convert target wind speed and direction to (Ug, Vg)
- Run 1D SCM with current (Ug, Vg)
- Extract wind speed at reference height
- Scale (Ug, Vg) to reduce error: `Ug_new = Ug * (target_speed / current_speed)^0.5`
- Repeat until convergence

### Step 2: 1D Profile Evolution

The 1D SCM runs a time-dependent simulation:
1. **Initialization**: Uniform horizontal winds = (Ug, Vg), linear temperature with lapse rate
2. **Time-stepping**: For each level `i`:
   - Compute surface layer (friction velocity, boundary layer mixing)
   - Update u-wind: `du/dt = diffusion + Coriolis + geostrophic balance + damping`
   - Update v-wind: `dv/dt = diffusion + Coriolis + geostrophic balance + damping`
   - Update temperature: `dT/dt = diffusion`
   - Update TKE: `dTKE/dt = production - dissipation + diffusion`
3. **Adaptive time-stepping**: `dt = 0.8 * dz / max(|U|)`
4. **Convergence check**: Stop when wind field changes by < 0.01 m/s

### Step 3: 3D Mapping

The converged 1D profile is mapped to 3D terrain-aligned coordinates:
- For each (i, j, k) grid point:
  - Compute height above local terrain: `z_agl = z_physical - terrain[i,j]`
  - Find nearest level in 1D profile
  - Assign velocity and temperature from 1D profile

## Boundary Conditions

Consistent with the reference Python solver:

### Wind Equations
```
du/dt = nut * d²u/dz² + 0.5/dz * (dnut/dz) * (du/dz) 
        + f * v - f * vg + coeff * (ug - u) / 20

dv/dt = nut * d²v/dz² + 0.5/dz * (dnut/dz) * (dv/dz) 
        - f * u + f * ug + coeff * (vg - v) / 20
```

Where:
- `f` = Coriolis parameter (depends on latitude)
- `nut` = eddy viscosity (m²/s)
- `coeff` = height-dependent damping coefficient (0 at top, 1 near surface)
- Damping coefficient varies smoothly from 0 at `z > z_top - 150 m` to 1 below `z_top - 100 m`

### Surface Layer (Monin-Obukhov)
- Friction velocity: `u* = κ * M1 / ln((z1 + z0) / z0)` where M1 is wind at first level
- Eddy viscosity: `nut = u* * κ * z0 / φm`

### Eddy Viscosity
```
nut = cmu * sqrt(tke) * lscale

lscale = 1 / sqrt(1/lshear² + 1/lmax²)  (for neutral/unstable)
       = lshear * sqrt(1 - Ri * (lshear/lmax)²)  (for stable)

lshear = κ * (z - z_lower)
lmax = 0.00027 * sqrt(Ug² + Vg²) / |f|  (Blackadar length scale)
```

### Temperature
```
dT/dt = d/dz(nut/σt * dT/dz)

σt = 1.0  (Prandtl number for heat)
```

### TKE (1-equation model)
```
dTKE/dt = production + buoyancy - dissipation + diffusion

production = nut * ((du/dz)² + (dv/dz)²)
dissipation = Ce * TKE^1.5 / lscale
Ce ≈ 1.92
```

## Usage

### C++ API

```cpp
// Inputs file settings
init_mode = scm
scm_wind_speed = 10.0           // Wind speed [m/s]
scm_wind_direction = 270.0      // Direction [degrees]
scm_ref_height = 10.0           // Reference height [m AGL]
scm_ref_temperature = 288.15    // Temperature [K]
scm_lapse_rate = 0.0065         // Lapse rate [K/m]
scm_domain_height = 4000.0      // Domain height [m]
scm_dz = 4.0                    // Grid spacing [m]
```

### Python API

```python
from wind_solver import WindSolver

# Initialize solver with SCM mode
wind = WindSolver("scm_inputs.i")

# Solve for wind field
wind.solve()

# Extract velocity at specified height
vel_10m = wind.get_velocity_at_agl(10.0)

# Save results
wind.write_plotfile("plt_scm")
wind.finalize()
```

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `scm_wind_speed` | Real | 10.0 | Wind speed at reference height [m/s] |
| `scm_wind_direction` | Real | 270.0 | Wind direction [degrees, 0=N, 90=E, 180=S, 270=W] |
| `scm_ref_height` | Real | 10.0 | Height where wind speed is specified [m AGL] |
| `scm_ref_temperature` | Real | 288.15 | Reference temperature at surface [K] |
| `scm_lapse_rate` | Real | 0.0065 | Temperature lapse rate [K/m] |
| `scm_domain_height` | Real | 4000.0 | Domain height for 1D SCM [m] |
| `scm_dz` | Real | 4.0 | Vertical grid spacing for 1D SCM [m] |

## Output

After convergence, the following geostrophic wind components are stored:
- `scm_ug` - Geostrophic u-component [m/s]
- `scm_vg` - Geostrophic v-component [m/s]

These can be accessed after initialization via the WindSolver API.

## Example

See `example_scm_inputs.i` for a complete example configuration.

## Performance Considerations

1. **1D SCM runs before 3D solve**: The SCM completes in seconds to minutes depending on convergence
2. **No additional cost to 3D solver**: 1D profile is simply mapped to 3D MultiFab
3. **Vertical resolution**: Using dz=4m is recommended for typical PBL heights
4. **Domain height**: 4 km is usually sufficient for decoupling from top boundary

## References

1. Hripko, G., et al. "A 1-D Column Model for Terrain-Aware Wind Field Generation" (2024)
2. Högström, U. "Review of some basic characteristics of the atmospheric surface layer." Boundary-Layer Meteorology 78.3 (1996): 215-246.
3. Blackadar, A. K. "The vertical distribution of wind and turbulent exchange in a neutral atmosphere." Journal of Geophysical Research 67.8 (1962): 3095-3102.
