# SCM (Single Column Model) Wind Initialization

## Overview

The Single Column Model (SCM) initialization provides a physics-based approach to initialize the 3D wind field using 1D atmospheric boundary layer simulation. The SCM runs a 1D column at each 3D grid point, converging the geostrophic wind to match a target wind speed at a reference height.

This approach is based on the Python reference implementation:
https://github.com/hgopalan/onedterrainsolver/blob/main/hrrr_1dsolver_terrain.py

## Physics

The SCM solves the 1D atmospheric boundary layer equations:

### Momentum Equations
```
∂u/∂t = ν_t ∂²u/∂z² + 0.5(∂ν_t/∂z)(∂u/∂z) + f(v - v_g) + damping + forcing
∂v/∂t = ν_t ∂²v/∂z² + 0.5(∂ν_t/∂z)(∂v/∂z) - f(u - u_g) + damping + forcing
```

Where:
- ν_t: turbulent eddy viscosity
- f: Coriolis parameter
- (u_g, v_g): geostrophic wind components
- damping: relaxation to geostrophic wind in upper domain
- forcing: optional boundary layer height-based forcing

### Temperature Equation
```
∂T/∂t = (ν_t/σ_T) ∂²T/∂z² + (∂/∂z)[(ν_t/σ_T)(∂T/∂z)]
```

### Turbulence Closure (1-Equation TKE Model)
```
∂TKE/∂t = ν_t (∂u/∂z)² + ν_t (∂v/∂z)² - buoyancy - dissipation + diffusion
dissipation = C_μ^3 TKE^1.5 / l_scale
```

With stability-dependent length scale:
```
l_max = 0.00027 √(u_g² + v_g²) / f
l_shear = κ(z - z_terrain) (modulated by PBL height)
l_scale² = l_max² + l_shear²
```

### Similarity Theory

Monin-Obukhov similarity is used to compute surface boundary conditions. Four heat flux modes are supported:

1. **Mode 1 (Heat Flux)**: Specified surface heat flux Q_h
2. **Mode 2 (Surface Temperature)**: Specified surface temperature T_s
3. **Mode 3 (Heating Rate)**: Specified temperature change rate dT/dt
4. **Mode 4 (MOL)**: Specified Monin-Obukhov length L (typical for operational runs)

For each mode, the similarity theory computes:
- Friction velocity: u* = κ M₁ / (ln(z₁/z₀) - ψ_m)
- Surface temperature or heat flux (depending on mode)
- Monin-Obukhov length: L = -u*³T_ref / (κg Q_h)
- Stability corrections: ψ_m, ψ_h (based on ζ = z/L)

## Convergence Algorithm

The SCM uses an iterative procedure to find the geostrophic wind (u_g, v_g) that produces the target wind at the reference height:

1. **Initialize** with guess u_g, v_g (default: 15, -10 m/s)
2. **Run 1D simulation** for scm_simulation_time with forward Euler time stepping
3. **Evaluate wind** at reference height z_ref
4. **Check convergence**: |u(z_ref) - u_target| < tolerance AND |v(z_ref) - v_target| < tolerance
5. If **NOT converged** and iterations < scm_max_iterations:
   - **Adjust geostrophic wind**: u_g ← u_g - 0.6×error_u, v_g ← v_g - 0.6×error_v
   - **Reset 1D fields** to initial conditions
   - **Go to step 2**
6. **Extract profile** and interpolate to 3D domain

The proportional feedback factor (0.6) provides stable convergence for most cases.

## 3D Interpolation

After the 1D SCM converges, the profiles are interpolated to the 3D domain:

1. Extract 1D profile: u(z), v(z), T(z), TKE(z), ν_t(z) on SCM grid (dz_scm = 4m typically)
2. For each 3D cell at position (x_i, y_j, k):
   - Compute height above ground: z_agl = z_k - z_terrain(x_i, y_j)
   - Interpolate to z_agl using linear interpolation on SCM grid
   - Store interpolated values in 3D field

This ensures terrain-aware initialization: cells above higher terrain start at different z_agl.

## Input Parameters

Add these parameters to your `inputs.i` file:

```fortran
# SCM Wind Initialization Parameters

# Initialization mode must be "scm"
init_mode = scm

# Target wind speed at reference height (m/s)
U_ref = 10.0
V_ref = 0.0

# Reference height for convergence (m)
# Typically 10m or measurement mast height
scm_z_ref = 150.0

# Surface roughness (m)
z0 = 0.1

# 1D SCM grid parameters
scm_height = 2000.0        # Maximum height for 1D simulation (m)
scm_dz = 4.0               # Grid spacing for 1D column (m)

# Latitude (degrees) - used for Coriolis parameter
scm_latitude = 45.0

# Heat flux mode: 1=heat_flux, 2=surface_temp, 3=heating_rate, 4=MOL
scm_heat_flux_mode = 4

# Value for specified heat flux mode
# For mode 4 (MOL): -1e30 = neutral, positive = stable, negative = unstable
scm_heat_flux_value = -1.0e30

# Reference temperature (K)
scm_temperature_reference = 300.0

# Surface temperature (K) - only used if scm_heat_flux_mode == 2
scm_temperature_surface = 300.0

# Convergence tolerance for geostrophic wind iteration (m/s)
scm_convergence_tolerance = 0.25

# Simulation time for single SCM run (seconds)
scm_simulation_time = 20000.0

# Maximum number of geostrophic wind iterations
scm_max_iterations = 100
```

## Example Output

```
================================================================================
SCM 1D Solver - Final Convergence Report
================================================================================
Specified wind (target):     u=10.000000 m/s, v=0.000000 m/s
Final wind at z_ref=150.000000m: u=10.053173 m/s, v=-0.102996 m/s
Error in u-wind:             0.053173 m/s
Error in v-wind:             0.102996 m/s
Total wind error:            0.115912 m/s
Final geostrophic wind:      ug=10.053477 m/s, vg=-0.066995 m/s
Convergence iterations:      5
Convergence tolerance:       0.250000 m/s
Status:                      CONVERGED ✓
================================================================================

wind_solver: SCM converged:
  Monin-Obukhov Length: -1e+30 m
  Friction Velocity: 0.870891 m/s
  Sensible Heat Flux: 4.92675e-29 W/m^2
```

## Regression Test

A regression test is provided in `regtest/scm_initialization/`:

```bash
cd regtest/scm_initialization
# Run solver
../../../build/wind_solver inputs_scm_test.i

# Validate convergence
python3 validate_scm.py
```

The test case uses:
- Flat terrain (all z=0)
- Target wind: [10, 0] m/s at z_ref=150m
- Neutral stability (MOL=-1e30)
- Convergence tolerance: 0.25 m/s

Expected result: Wind at 150m within [0.053, 0.103] m/s error.

## Implementation Details

### Files
- `src/scm_1d_solver.H`: SCM solver class declaration
- `src/scm_1d_solver.cpp`: SCM solver implementation (500+ lines)
- `src/wind_solver_app.H`: WindSolverApp integration (11 SCM parameters)
- `src/wind_solver_app.cpp`: SCM initialization in initialize_wind_fields()

### Key Methods
- `SCM1DSolver::run_to_convergence()`: Main iteration loop
- `SCM1DSolver::compute_similarity()`: Monin-Obukhov calculations
- `SCM1DSolver::update_windspeed_x/y()`: Momentum update
- `SCM1DSolver::update_temperature()`: Temperature evolution
- `SCM1DSolver::update_turbulence()`: TKE equation
- `SCM1DSolver::adjust_geostrophic_wind()`: Feedback adjustment

### Time Stepping
- Adaptive forward Euler with CFL criterion: dt = 0.8 × dz / max(|u|, |v|)
- Typical simulation time: 20000 seconds (5.5 hours)
- Typical convergence: 100-5000 iterations depending on convergence tolerance

## Physics Validation

The SCM implementation follows the Python reference exactly:
1. Grid: z ∈ [0, 2000m] with dz=4m (500+ grid points)
2. Coriolis: f = 2Ω sin(lat) at 45° latitude
3. Similarity theory: Unified treatment for all heat flux modes
4. Stability corrections: Holtslag & De Bruin (1988) functions
5. Turbulence closure: Cmu formulation with Richardson number effects

## Known Limitations

1. **No moisture**: Currently dry boundary layer only
2. **No precipitation/clouds**: Clear-sky assumption
3. **No surface roughness variation**: Uniform z₀ across domain
4. **1D column assumption**: Each 3D point treated independently (no lateral advection)
5. **Neutral to moderately stable**: May not handle very strong inversions well

## Future Enhancements

- [ ] Moisture transport (q equation)
- [ ] Coupled soil model for surface temperature
- [ ] Spatially-varying z₀ based on land use
- [ ] Urban canopy effects (3D to 1D parameter adjustment)
- [ ] Interactive spectral analysis for power-law profiles

## References

1. Python reference: https://github.com/hgopalan/onedterrainsolver
2. Holtslag, A. A. M., and B. A. De Bruin, 1988: Applied Modeling of the Nighttime Surface Energy Balance over Land. J. Appl. Meteor., 27, 689–704.
3. Businger, J. A., J. C. Wyngaard, Y. Izumi, and E. F. Bradley, 1971: The effect of thermal stratification on turbulence in the boundary layer. J. Atmos. Sci., 28, 190–209.
4. Högström, U., 1988: Non-dimensional wind and temperature profiles in the atmospheric surface layer: a re-evaluation. J. Atmos. Sci., 45, 1879–1883.

## Contact

For questions or issues with SCM initialization, refer to the GitHub repository issues:
https://github.com/hgopalan/massconsistent_amr/issues
