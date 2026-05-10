# massconsistent_amr

A terrain-following, mass-consistent 3-D wind solver built on [AMReX](https://amrex-codes.github.io/amrex/).

## Overview

`massconsistent_amr` implements the QUIC-URB style mass-consistent wind adjustment method (Sherman 1978).  Given a reference wind speed and a terrain elevation file, the solver:

1. Constructs a log-law initial wind field over the terrain using per-column height-above-ground-level (AGL).
2. Enforces mass consistency (∇·**u** = 0) by solving the anisotropic Poisson equation via the AMReX MLMG linear solver (MLABecLaplacian).
3. Writes the corrected divergence-free wind field as an AMReX plotfile and, optionally, a terrain-aligned CSV slice.

The governing equation is:

```
-(α_h² ∂²λ/∂x² + α_h² ∂²λ/∂y² + α_v² ∂²λ/∂z²) = -(∇·u₀)
```

where λ is the Lagrange multiplier and α_h, α_v are horizontal and vertical anisotropy factors.

## Dependencies

- **AMReX** ≥ 24.x (vendored as a git submodule under `external/amrex`)
- **CMake** ≥ 3.20
- **C++17** compiler (GCC, Clang, MSVC, or Intel icpx)

## Building

```bash
# Clone the repository including the AMReX submodule
git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
cd massconsistent_amr

# Configure and build (CPU-only, Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The `wind_solver` executable will be placed in `build/`.

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `MASSCONSISTENT_USE_VENDORED_AMREX` | `ON` | Use the bundled AMReX submodule |
| `MASSCONSISTENT_ENABLE_MPI` | `OFF` | Enable MPI parallelism |
| `MASSCONSISTENT_GPU_BACKEND` | `NONE` | GPU backend: `NONE`, `CUDA`, `HIP`, or `SYCL` |

## Running

```bash
./build/wind_solver regtest/gaussian_hill/inputs.i
```

Key input parameters (set in the `inputs.i` file):

| Parameter | Default | Description |
|-----------|---------|-------------|
| `terrain_file` | `terrain.csv` | X Y Z terrain point file |
| `U_ref` / `V_ref` | `10.0` / `0.0` | Reference wind components [m/s] |
| `z_ref` | `10.0` | Reference height above local terrain [m] |
| `z0` | `0.1` | Aerodynamic roughness length [m] |
| `dx` / `dy` / `dz` | `30.0` | Grid spacing [m] |
| `domain_height` | `300.0` | Vertical extent above max terrain [m] |
| `alpha_h` / `alpha_v` | `1.0` | Lagrange anisotropy factors |
| `plot_file` | `plt_wind` | Output plotfile prefix |
| `extract_agl` | `-1.0` | AGL height for CSV slice output [m]; <0 disables |
| `extract_file` | `wind_extract.csv` | Terrain-aligned CSV output filename |

## Regression Tests

Regression tests are located in `regtest/`.  After building, run them with:

```bash
ctest --test-dir build -L regtest --output-on-failure
# or
cmake --build build --target regtest
```

| Test | Description |
|------|-------------|
| `flat_terrain` | Flat z=0 domain (2×2×2 grid); verifies MLMG convergence on trivial geometry |
| `gaussian_hill` | Gaussian hill (peak 50 m, 10×10×6 grid); verifies terrain-following log-law wind |

## References

- Sherman, C. A. (1978). *A mass-consistent model for wind fields over complex terrain.* Journal of Applied Meteorology, 17(3), 312–319.
- AMReX: [https://github.com/AMReX-Codes/amrex](https://github.com/AMReX-Codes/amrex)
