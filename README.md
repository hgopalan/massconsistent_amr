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
- **Multiple initialization modes** — Log-law, uniform, RAWS stations, or HRRR-style surface parameters
- **Building support** — Wake modeling with Röckle (1990) parameterization
- **Canopy modeling** — Forest canopy drag effects
- **Tunable multigrid solver** — Configurable MLMG parameters and bottom solver selection for performance optimization
- **Performance timing** — Detailed timing output for profiling and optimization
- **Gaussian puff dispersion** — Passive pollutant transport
- **Python API** — Coupling with fire and atmospheric models
- **GPU-ready** — Runs on NVIDIA, AMD, and Intel GPUs via AMReX

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
