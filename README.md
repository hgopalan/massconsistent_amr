# massconsistent_amr

An AMReX-based C++ mass-consistent 3-D wind diagnostic solver providing terrain-following wind field adjustment with GPU-ready kernels (CUDA/HIP/SYCL), building wake parameterization, canopy effects, and optional MPI parallelism.

## CI / Build Status

| Configuration | Status |
|---------------|--------|
| Linux & macOS CPU (GCC/Clang, Release + Debug) | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Windows CPU (MSVC, Release + Debug) | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Linux GPU — CUDA 12.6 | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Windows GPU — CUDA 12.6 | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Linux GPU — HIP/ROCm 6.2 | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Linux GPU — SYCL/oneAPI 2025.x | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Documentation | [![Build and Deploy Documentation](https://github.com/hgopalan/massconsistent_amr/actions/workflows/docs.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/docs.yml) |

📖 **[Full documentation](https://hgopalan.github.io/massconsistent_amr/)**

## Quick Start

```bash
git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
cd massconsistent_amr
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/wind_solver regtest/gaussian_hill/inputs.i
```

## Features

- **Mass-consistent wind solver** — Enforces ∇·u = 0 using Lagrange multiplier approach
- **Terrain-following** — Log-law wind profiles over complex topography
- **Multiple initialization modes** — Log-law, uniform, RAWS stations, HRRR-style surface parameters, or power-law profiles
- **Position-dependent roughness** — Spatially-varying aerodynamic roughness length z₀ from file
- **Building support** — Wake modeling with Röckle (1990) and Huber-Snyder (EPA) parameterizations
- **Canopy modeling** — Forest canopy drag effects
- **Height-dependent anisotropy** — Vertical adjustment coefficient α_v can vary with height
- **Non-neutral stability corrections** — Businger-Dyer Monin-Obukhov profiles for stable/unstable conditions
- **Elevation-dependent wind scaling** — Terrain elevation effects on reference wind speed for mountain-valley flows
- **Orographic speed-up** — Jackson & Hunt (1975) model for hill/ridge acceleration and lee-side flow separation
- **Sea breeze parameterization** — Thermal circulation from land-sea temperature contrast driving onshore/offshore winds
- **Froude number terrain blocking** — Flow blocking and channeling around steep terrain in stable stratification
- **Katabatic/Anabatic slope flows** — Thermally-driven up-slope (daytime) and down-slope (nighttime) flows on inclined terrain
- **Gap flow parameterization** — Pressure-driven channeling through mountain gaps/passes with 2-4× wind speed enhancement
- **Time-varying boundary conditions** — Support for transient wind simulations with time series input
- **Building porosity model** — Porous flow through structures (trees, fences) with drag parameterization
- **Thermal stratification with buoyancy** — Temperature-driven buoyancy effects on vertical velocity using Boussinesq approximation
- **Kinematic terrain-following BC** — No-flow-through condition w = u·∇h at terrain surface for improved terrain representation
- **Tunable multigrid solver** — Configurable MLMG parameters and bottom solver selection for performance optimization
- **Performance timing** — Detailed timing output for profiling and optimization
- **Gaussian puff dispersion** — Passive pollutant transport with enhanced physics:
  - **Height-dependent diffusivity** — K(z) profiles for realistic atmospheric mixing
  - **First-order decay** — Exponential decay for radioactive/chemical species
  - **Plume rise** — Briggs buoyancy formula for heated sources (WindNinja/QUIC-PLUME compatible)
- **Enhanced diagnostics** — Surface heat flux and drag coefficient output fields
- **Python API** — Coupling with fire and atmospheric models
- **GPU-ready** — Runs on NVIDIA, AMD, and Intel GPUs via AMReX
- **New features:**
  - **Divergence source terms** — Non-zero RHS (∇·u = S) for convective plumes and mass sources
  - **Holtslag-De Bruin stability functions** — Alternative formulation for very stable conditions
  - **Land use-based power-law exponents** — Spatially-varying wind shear by terrain type
  - **Diurnal temperature profile** — Time-varying sinusoidal temperature for daily cycles
  - **Vegetation roughness factor** — Seasonal/state modifiers for roughness (LAI, snow, burn severity)

## Documentation

See the [full documentation](https://hgopalan.github.io/massconsistent_amr/) for:

- Building and installation instructions
- Input parameter reference
- Performance tuning guide
- Usage examples and tutorials
- Wind initialization modes (log-law, uniform, RAWS, surface_data for HRRR)
- Buildings and wake effects
- Canopy modeling
- Puff dispersion model
- Python API reference
- Regression tests

## License

See [LICENSE](LICENSE).
