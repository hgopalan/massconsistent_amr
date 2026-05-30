# Gaussian Puff Model Implementation for massconsistent_amr

## Overview

This implementation adds a **Gaussian Puff dispersion model** to the massconsistent_amr wind solver. The puff model computes passive plume dispersion for pollutants emitted from point sources in a steady wind field.

**NEW (May 2026)**: The puff model now includes comprehensive support for **terrain**, **buildings**, and **tree canopy** effects, enabling realistic dispersion simulations in complex environments.

## Files Added/Modified

### 1. Core Model Implementation

- **`src/puff_models.H`** (~600 lines, enhanced)
  - Header-only library with GPU-compatible kernels
  - Data structures: `Puff`, `PuffParams`, `TerrainGrid`
  - **NEW**: Integration with `wake_models.H` and `canopy_models.H`
  - Key functions:
    - `gaussian_puff_concentration()` - Compute 3D Gaussian concentration
    - `gaussian_puff_concentration_with_reflection()` - **NEW**: Image source method
    - `interpolate_terrain_height()` - **NEW**: Terrain elevation interpolation
    - `advect_puff()` / `advect_puff_with_terrain()` - Drift with wind, ground reflection
    - `update_puff_growth()` / `update_puff_growth_with_wake()` - Diffusive growth
    - `point_in_building()` / `point_in_any_building()` - **NEW**: Building collision
    - `compute_wake_enhancement_factor()` - **NEW**: Wake-enhanced diffusivity
    - `compute_canopy_diffusivity()` - **NEW**: Canopy turbulence effects
    - `apply_canopy_deposition()` - **NEW**: Dry deposition in canopy
    - `update_puff_age()` - Track puff lifetime
    - `check_puff_bounds()` / `check_puff_bounds_with_terrain()` - Domain checks
    - `create_puff()` - Emit new puff from source

### 2. Standalone Solver

- **`src/puff_solver.cpp`** (~530 lines, enhanced)
  - Standalone executable for puff dispersion
  - **NEW**: Reads terrain, building, and canopy data
  - **NEW**: Integrated terrain/building/canopy effects in time loop
  - Reads input parameters via AMReX ParmParse
  - Main time-stepping loop with full feature integration
  - Concentration gridding and output (CSV format)
  - Easy to test and validate independently

### 3. Build Configuration

- **`CMakeLists.txt`** (updated)
  - Added `puff_solver` executable target
  - Linked with AMReX
  - GPU-aware compilation for CUDA/HIP/SYCL

### 4. Test Cases

- **`regtest/puff_gaussian/`** - Simple uniform wind test (original)
- **`regtest/puff_terrain/`** - **NEW**: Ground reflection over Gaussian hill
- **`regtest/puff_buildings/`** - **NEW**: Wake-enhanced dispersion around buildings
- **`regtest/puff_canopy/`** - **NEW**: Canopy diffusivity and deposition
- **`regtest/puff_coupled_full/`** - **NEW**: Terrain + buildings + canopy combined

### 5. Documentation

- **`docs/puff.rst`** (~380 lines, updated)
  - Physical model description with equations
  - Implementation details and algorithms
  - **NEW**: Terrain/building/canopy integration documentation
  - Parameter documentation
  - Usage examples
  - Validation approach
  - Test case descriptions
  - References (added Röckle, Shaw-Pereira, MacDonald)

## Model Equations

### Gaussian Puff Concentration

Each puff contributes to the total concentration via:

$$C_i(x, y, z) = \frac{m_i}{(2\pi)^{3/2} \sigma_x \sigma_y \sigma_z} \exp\left(-\frac{dx^2}{2\sigma_x^2} - \frac{dy^2}{2\sigma_y^2} - \frac{dz^2}{2\sigma_z^2}\right)$$

Total concentration is the superposition:
$$C(x, y, z) = \sum_i C_i(x, y, z)$$

### Puff Evolution

1. **Advection**: $\mathbf{r}_i^{n+1} = \mathbf{r}_i^n + \mathbf{u}(\mathbf{r}_i) \Delta t$
2. **Growth**: $\sigma(t) = \sqrt{\sigma_0^2 + 2K \cdot t}$
3. **Age**: $\text{age} \gets \text{age} + \Delta t$

## Key Features

✅ **GPU-Portable**: Uses AMReX AMREX_GPU_DEVICE macros  
✅ **Modular Design**: Puff model in separate header file  
✅ **Flexible I/O**: ParmParse for input, CSV for output  
✅ **Easy to Test**: Standalone solver with uniform wind  
✅ **Well-Documented**: Equations, parameters, validation approach  
✅ **Extensible**: Easy to add settling, decay, deposition  

## Quick Start

### Build

```bash
cd massconsistent_amr
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Run Puff Solver

```bash
./build/puff_solver regtest/puff_gaussian/inputs.i
```

### Expected Output

```
puff_solver: Gaussian puff model enabled
  Source: (150, 150, 10)
  Emission rate: 1.0 units/s
  Emission duration: 50.0 s
  K_h = 1.0 m²/s, K_v = 0.5 m²/s
  Initial puff size: σy₀ = 1.0 m, σz₀ = 1.0 m
  Wind: U = 10.0, V = 0.0, W = 0.0 m/s
  Time steps: 100 @ dt = 0.5 s
  Grid: 30 x 30 x 10 (10 x 10 x 10 m)
  Step 0 (t = 0.0 s): 1 puffs
    Wrote concentration to puff_concentration.csv_step0
  Step 10 (t = 5.0 s): 11 puffs
    Wrote concentration to puff_concentration.csv_step10
  ...
puff_solver: done.
  Total puffs emitted: 100
  Active puffs at end: 100
```

## Input Parameters

### Source and Emission

```ini
enable_puff = true
source_x = 150.0      # [m]
source_y = 150.0      # [m]
source_z = 10.0       # [m]
emission_rate = 1.0   # [units/s]
emission_duration = 50.0  # [s]
```

### Diffusivity (Gaussian Growth)

```ini
K_h = 1.0             # Horizontal [m²/s]
K_v = 0.5             # Vertical [m²/s]
sigma_y0 = 1.0        # Initial lateral width [m]
sigma_z0 = 1.0        # Initial vertical height [m]
```

### Wind Field

```ini
U_wind = 10.0         # x-component [m/s]
V_wind = 0.0          # y-component [m/s]
W_wind = 0.0          # z-component [m/s]
```

### Domain and Grid

```ini
xmin = 0.0,   xmax = 300.0
ymin = 0.0,   ymax = 300.0
zmin = 0.0,   zmax = 100.0
dx = 10.0,  dy = 10.0,  dz = 10.0
```

### Time Stepping

```ini
dt_puff = 0.5         # [s]
n_steps_puff = 100    # Total steps
output_freq_puff = 10 # Write every N steps
```

## Output Format

Concentration files are CSV with columns: `x, y, z, C`

Example: `puff_concentration.csv_step10`
```
# Gaussian puff concentration field (step 10)
# x [m], y [m], z [m], C [units/m³]
0.0,0.0,5.0,0.000000e+00
10.0,0.0,5.0,1.234e-12
20.0,0.0,5.0,2.456e-10
...
```

## Validation Against Analytical Solution

For a **continuous point source** in **uniform wind**, the steady-state Gaussian plume is:

$$C(x, y, z) = \frac{Q}{2\pi u \sigma_y \sigma_z} \exp\left(-\frac{y^2}{2\sigma_y^2}\right) \left[\exp\left(-\frac{z^2}{2\sigma_z^2}\right) + \exp\left(-\frac{(2H-z)^2}{2\sigma_z^2}\right)\right]$$

To validate the puff model:
1. Run with very long `emission_duration` (e.g., 10,000 s)
2. Extract concentration at downwind distances
3. Compare with analytical Gaussian plume solution
4. Check that plume growth matches theory: σ ∝ √(K·t)

## Implementation Details

### Algorithm

```
Initialize empty puff list

for t = 0 to n_steps:
  # Emission
  if t < emission_duration / dt:
    puff_mass = emission_rate * dt
    emit_puff(source, puff_mass)
  
  # Advection, Growth, Aging
  for each puff:
    if puff.active:
      advect with wind
      grow due to diffusion
      update age
      check if outside domain
  
  # Concentration Computation
  if t % output_freq == 0:
    for each grid point (i,j,k):
      C[i,j,k] = sum over all puffs of gaussian_concentration(i,j,k)
    write_csv(concentration_grid, t)
```

### Data Structures

**Puff**
- Position: x, y, z
- Size: sigma_y, sigma_z
- Mass: cumulative emission
- Age: time since emission
- Active: boolean flag

**PuffParams**
- Source location and emission strength
- Diffusivity parameters (K_h, K_v)
- Initial puff size (sigma_y0, sigma_z0)

## Computational Complexity

- **Per Puff**: O(1) - only update position and size
- **Per Timestep**: O(N_puffs × N_grid) to compute concentrations
  - N_puffs grows linearly with time
  - Example: 100 puffs, 30×30×10 grid = 900k operations
- **Memory**: O(N_puffs) for puff list + O(N_grid) for concentration field

For the test case: ~100 puffs, small grid → very fast (<1 second)

## Limitations and Future Work

### Current Status (May 2026)

**Implemented Features** ✅
1. ✅ **Ground reflection**: Image source method for terrain-aware reflection
2. ✅ **Building masking**: Puffs deactivated inside building volumes
3. ✅ **Wake-enhanced diffusivity**: Röckle model for cavity and far wake zones
4. ✅ **Canopy diffusivity**: Enhanced vertical mixing, reduced horizontal in canopy
5. ✅ **Canopy deposition**: Dry deposition for particles/aerosols in vegetation

**Remaining Limitations** ❌
1. ❌ **Nearest-neighbor interpolation**: Should use trilinear velocity interpolation for spatially-varying winds
2. ❌ **No chemical decay**: No radioactive or chemical decay modeled
3. ❌ **No plume rise**: No buoyancy effects for heated sources
4. ❌ **Spatially uniform canopy**: Canopy properties are domain-wide constants

### Future Extensions (Priority Order)

| Feature | Effort | Benefit | Priority |
|---------|--------|---------|----------|
| Trilinear velocity interpolation | Moderate | Accurate advection in complex wind | High |
| Couple with wind plotfile | Moderate | Use real wind fields from solver | High |
| Spatially-varying canopy | Easy | Realistic heterogeneous vegetation | Medium |
| Chemical decay (1st-order) | Easy | Reactive species dispersion | Medium |
| Plume rise (buoyancy) | Moderate | Heated/buoyant sources | Medium |
| Particle settling | Easy | Size-dependent aerosol deposition | Low |
| Python API | Easy | Coupled wildfire-smoke simulations | Future |

## Testing and Benchmarking

### Test Cases

**1. puff_gaussian** - Baseline uniform wind test
- **Input**: Point source at (150, 150, 10) emitting 1 unit/s for 50 s  
- **Wind**: 10 m/s from west (U=10, V=W=0)  
- **Domain**: 300 m × 300 m × 100 m  
- **Purpose**: Validate basic puff advection and growth

**2. puff_terrain** - Ground reflection over Gaussian hill
- **Terrain**: 50m peak at center, 300m × 300m domain
- **Source**: Upwind at 20m elevation
- **Purpose**: Validate terrain reflection and image source method

**3. puff_buildings** - Wake effects around single building
- **Building**: 50m × 40m × 30m tall at domain center
- **Wake**: 3x diffusivity in cavity, 1.5x in far wake
- **Purpose**: Validate building masking and wake enhancement

**4. puff_canopy** - Forest canopy effects
- **Canopy**: 20m tall uniform forest
- **Effects**: 3x vertical diffusivity, 0.7x horizontal, deposition
- **Purpose**: Validate canopy turbulence and deposition

**5. puff_coupled_full** - All features combined
- **Terrain**: Gaussian hill (40m peak)
- **Buildings**: 3 buildings of varying heights
- **Canopy**: 15m forest with deposition
- **Domain**: 400m × 400m × 150m
- **Purpose**: Validate full integration in complex environment

### Expected Results

**Baseline (puff_gaussian)**:
- Plume center drifts 500m downwind in 50s (50s × 10 m/s)
- Puffs grow: σ_y(t) = √(1² + 2×1×t) meters
- ~100 active puffs at end

**Terrain (puff_terrain)**:
- Puffs reflect when approaching ground
- Concentration enhanced near terrain surface (image sources)
- No puffs penetrate below terrain elevation

**Buildings (puff_buildings)**:
- Puffs deactivated inside building volume (masking)
- Enhanced spreading in wake zones (3x in cavity, 1.5x in far wake)
- Plume widens significantly downwind of building

**Canopy (puff_canopy)**:
- Increased vertical mixing within canopy (3x K_v)
- Reduced horizontal spreading within canopy (0.7x K_h)
- Mass decreases due to deposition (exponential decay)
- Puffs above canopy show normal diffusion

**Coupled (puff_coupled_full)**:
- Combined effects: terrain reflection + building wakes + canopy deposition
- Complex concentration patterns influenced by all features
- Realistic urban/wildland dispersion simulation
- **Domain**: 400m × 400m × 150m
- **Purpose**: Validate full integration in complex environment
**Duration**: 50 s with Δt=0.5 s (100 timesteps)  

**Expected Results**:
- Plume center drifts from x=150 to x=650 (500 m distance = 50 s × 10 m/s) ✓
- Puffs grow over time: σ_y(t) = √(1² + 2×1×t) [m]
- Concentration decreases due to spreading
- At t=50s, ~100 puffs active, spreading over ~600 m × 300 m × 50 m domain

## Code Quality

- ✅ Header-only model → no linking issues
- ✅ GPU-ready → uses AMREX_GPU_DEVICE macros
- ✅ Well-commented → equations in comments
- ✅ Modular → easy to test each function independently
- ✅ Documented → equations and parameters explained
- ✅ No external dependencies → only AMReX

## References

1. **Pasquill & Gifford (1961)** - Classic Gaussian plume dispersion
2. **Hanna et al. (1982)** - Handbook on atmospheric diffusion
3. **Pardyjak & Brown (2001)** - QUIC-URB model (similar approach)
4. **Stohl et al. (2005)** - FLEXPART Lagrangian particle model (reference)

## Next Steps

1. **Build and test**: `cmake --build build && ./build/puff_solver regtest/puff_gaussian/inputs.i`
2. **Visualize**: Parse CSV output, plot concentration field
3. **Validate**: Compare with analytical Gaussian plume solution
4. **Extend**: Add deposition, decay, coupling with wind field
5. **Integrate**: Couple with wind solver for full wind-plume simulation

---

**Author**: Created as part of massconsistent_amr project  
**Status**: Ready for testing and validation  
**Next Phase**: Couple with wind field from massconsistent solver
