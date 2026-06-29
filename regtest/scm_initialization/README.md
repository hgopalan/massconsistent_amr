# SCM (Single Column Model) Initialization Regression Test

## Overview

This regression test validates the C++ Single Column Model (SCM) initialization option in massconsistent_amr against the Python reference implementation:

**Reference:** https://github.com/hgopalan/onedterrainsolver/blob/main/hrrr_1dsolver_terrain.py

The SCM is a 1D column atmosphere solver that operates on an independent grid and runs to convergence by iteratively adjusting the geostrophic wind until the wind speed at a reference height matches the target value.

## Test Case Description

This test reproduces the "Coarse search" test case from the Python code with:

- **Domain**: Idealized flat terrain (no elevation)
- **1D Grid**: z = 0 to 2000 m with dz = 4 m
- **Reference Height**: 150 m above ground
- **Target Wind**: u = 10.0 m/s, v = -4.0 m/s (at reference height)
- **Heat Flux Mode**: 4 (Monin-Obukhov Length specified)
- **Stability**: Neutral (MOL = -1e30)
- **Latitude**: 45° (for Coriolis parameter)
- **Roughness**: z0 = 0.1 m

## Input Parameters

See `inputs_scm_test.i` for the full list. Key SCM parameters:

```
init_mode = scm                    # Use SCM initialization
scm_height = 2000.0               # 1D grid extent (m)
scm_dz = 4.0                       # 1D grid spacing (m)
scm_z_ref = 150.0                 # Reference height for convergence (m)
scm_heat_flux_mode = 4            # MOL mode
scm_heat_flux_value = -1.0e30     # Neutral stability
scm_convergence_tolerance = 0.25  # Convergence criterion (m/s)
scm_latitude = 45.0               # For Coriolis
```

## Physical Model

The SCM solves:

1. **Momentum equations** with Coriolis forcing and turbulent mixing
2. **Temperature equation** with stratification
3. **Turbulent closure**: 1-equation TKE model with stability-dependent mixing length

### Key Physics Features

- **Similarity Theory**: Monin-Obukhov similarity at the surface
- **Turbulence**: Kaimal/Businger parameterization
- **Boundary Layer**: Proper handling of surface layer and free atmosphere
- **Convergence**: Iterative adjustment of geostrophic wind to match target wind speed

## Expected Results

At the reference height (150 m), the solver should produce wind speeds converging to:
- u ≈ 10.0 m/s ± 0.25 m/s (convergence tolerance)
- v ≈ -4.0 m/s ± 0.25 m/s

### Convergence Process

The Python code implements the following algorithm:

```python
while (residualx > allowed_error or residualy > allowed_error):
    # Initialize simulation with current geostrophic wind
    amr1D.initialize_physics(ug, vg, ...)
    
    # Run 1D column simulation to convergence
    amr1D.run_simulation(num_of_steps, tolerance)
    
    # Evaluate wind at reference height
    met_mast_cfd_u = interpolate(scm_z, scm_ux, metMastHeight)
    
    # Adjust geostrophic wind
    if error > tolerance:
        ug += adjustment * sign(met_mast_cfd_u - target_u)
        vg += adjustment * sign(met_mast_cfd_u - target_u)
```

The C++ implementation follows this exact logic, adapted to the AMReX framework.

## 3D Interpolation

After the 1D SCM converges, the profiles are interpolated to the 3D domain:

1. For each 3D grid cell, calculate height above ground level (AGL)
2. Linearly interpolate 1D SCM profile at that AGL
3. Account for terrain elevation variations

This allows the initial 1D column solution to be mapped to a 3D domain with terrain.

## Validation

Run the regression test:

```bash
cd regtest/scm_initialization
python3 validate_scm.py
```

The validation script:
1. Reads the AMReX plotfile output
2. Extracts wind profiles at the reference height
3. Checks convergence: |u_actual - u_target| < tolerance
4. Validates physical consistency (monotonic wind speed increase, etc.)

## Test Case Equivalence to Python Code

| Item | Python | C++ |
|------|--------|-----|
| Grid Height | 2000 m | 2000 m |
| Grid Spacing | ~10 m | 4 m |
| Reference Height | 150 m | 150 m |
| Target Wind | (10, -4) m/s | (10, -4) m/s |
| Convergence Tolerance | 0.25 m/s | 0.25 m/s |
| Heat Flux Mode | 4 (MOL) | 4 (MOL) |
| Latitude | 45° | 45° |
| Turbulence Model | 1-eq TKE | 1-eq TKE |

## Files

- `inputs_scm_test.i`: AMReX input file with SCM parameters
- `validate_scm.py`: Python validation script to check convergence
- `README.md`: This file

## Reference Documentation

See the main massconsistent_amr documentation:
- `docs/SCM_INITIALIZATION.md` - Full SCM initialization guide
- `src/scm_1d_solver.H` - SCM solver class documentation

## Date Added

2026-06-29

## Comments

This test validates that:
1. The 1D SCM solver correctly implements the Python reference physics
2. The geostrophic wind iteration converges properly
3. The 3D interpolation of 1D profiles works correctly
4. No regressions in other initialization modes

The test should be run after any modifications to:
- `src/scm_1d_solver.H` or `src/scm_1d_solver.cpp`
- `src/wind_solver_app.cpp` (initialize_wind_fields or initialize_scm_profile)
