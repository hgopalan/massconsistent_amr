# Phase 5: Documentation & Examples - Synthetic Turbulence Tutorial

## Tutorial: From Mean Wind to OpenFAST Turbulent Fields

This tutorial guides you through the complete workflow of generating synthetic turbulent wind fields compatible with NREL OpenFAST wind turbine simulations.

### Workflow Overview

```
1. Configure mean wind field (mass-consistent solver)
              ↓
2. Define turbulence parameters (Phase 1)
              ↓
3. Generate random fields (Phase 2: FFT-based synthesis)
              ↓
4. Create time-series (Phase 3: temporal synthesis)
              ↓
5. Export to OpenFAST (BTS binary format)
              ↓
6. Validate fields (Phase 4: spectral/continuity checks)
              ↓
7. Visualize results (VTK conversion for ParaView)
```

## Step 1: Configure the Mean Wind Field

Start with a basic wind solver configuration:

```ini
# Terrain file (DEM or synthetic)
terrain_file = terrain.csv

# Reference wind at measurement height
U_ref = 10.0    # [m/s]
V_ref = 0.0     # [m/s]
z_ref = 10.0    # [m AGL]

# Aerodynamic roughness
z0 = 0.03       # [m] - open terrain

# Grid resolution (balance accuracy vs. computation)
dx = 30.0       # [m] - horizontal spacing
dy = 30.0       # [m]
dz = 25.0       # [m] - vertical spacing

# Domain height
domain_height = 100.0  # [m] above max terrain

# Solver options
mlmg_verbose = 0
max_grid_size = 32
```

## Step 2-5: Complete Example with All Phases

See the complete example input file at the end of this tutorial with all five phases configured.

## Step 6: Phase 4 - Validation

Validate generated fields against physical constraints:

```bash
cd build
ctest -L synthetic_turbulence_full -V
```

## Step 7: Visualization with ParaView

Convert BTS to VTK format for 3D visualization:

```bash
python3 tools/bts_to_vtk.py turbulence.bts turbulence.vtk
```

## Physics Reference: Spectrum Models

### Von Kármán Spectrum

**Equation:**
```
S(f) = (4 * L_u * σ_u²) / (1 + 70.8 * (f * L_u / U)²)^(5/6)
```

### Kaimal Spectrum

**Equation:**
```
S(f) = (4 * L_u * σ_u²) / (1 + 5 * f * L_u / U)²
```

## Physical Parameter Ranges

### Turbulence Intensity
| Condition | Range |
|-----------|-------|
| Over water | 0.06-0.10 |
| Open terrain | 0.10-0.15 |
| Complex terrain | 0.15-0.25 |
| Urban/forest | 0.20-0.30 |

## References

1. von Kármán, T. (1948). Progress in the statistical theory of turbulence.
2. Kaimal, J.C., et al. (1972). Spectral characteristics of surface-layer turbulence.
3. IEC 61400-1 (2019). Wind energy generation systems.
4. NREL TurbSim User's Guide
5. Pope, S.B. (2000). Turbulent Flows.
