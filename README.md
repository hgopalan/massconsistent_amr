# massconsistent_amr

**massconsistent_amr** is a high-performance, GPU-accelerated (CUDA/HIP/SYCL), and MPI-parallel C++ 3-D mass-consistent wind diagnostic solver built on the AMReX framework. It features advanced terrain-following adjustment with spatially-varying anisotropy, building/canopy drag, analytical turbine wake modeling, and advanced atmospheric dispersion (Lagrangian Puff and LPDM).

Key capabilities include mass-consistent wind fields over complex terrain, building wakes, canopy effects, synthetic turbulence, and dispersion modeling with reactive chemistry and deposition.

📖 **[Full documentation](https://hgopalan.github.io/massconsistent_amr/)**

## Quick Start

```bash
git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
cd massconsistent_amr
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_USE_VENDORED_AMREX=ON
cmake --build build --parallel
./build/wind_solver regtest/gaussian_hill/inputs.i
```

## Build Options

* `-DMASSCONSISTENT_GPU_BACKEND=[NONE|CUDA|HIP|SYCL]` — GPU acceleration (default: `NONE`)
* `-DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=[ON|OFF]` — Python API (default: `OFF`)
* `-DMASSCONSISTENT_ENABLE_MPI=[ON|OFF]` — MPI parallelism (default: `OFF`)
* `-DMASSCONSISTENT_USE_VENDORED_AMREX=[ON|OFF]` — Use vendored AMReX (default: `ON`)

## Features

See [Full documentation](https://hgopalan.github.io/massconsistent_amr/) for details on:

- **Mass-consistent wind fields** over complex terrain
- **Building and canopy drag** modeling  
- **Analytical turbine wakes** (Jensen, Bastankhah, TurbOPark)
- **Gaussian puff dispersion** with chemistry and deposition
- **Synthetic turbulence** (Kaimal, Von Kármán, Mann box)
- **Infrastructure loading** (bridges, transmission lines)
- **External coupling** (FLORIS, PyWake, wildfire)

## CI / Build Status

| Configuration | Status |
|---------------|--------|
| Linux & macOS CPU | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Windows CPU | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build_windows)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Linux GPU — CUDA | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build_cuda)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Linux GPU — HIP/ROCm | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build_hip)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Documentation | [![Build and Deploy Documentation](https://github.com/hgopalan/massconsistent_amr/actions/workflows/docs.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/docs.yml) |

## Test Cases & Validation

- **80+ Regression Tests**: Covering solver, wakes, turbulence, dispersion, and wildfire coupling
- **Phase 5 Validation Suite**: Multi-source, time-varying emissions, chemistry, and deposition tests
- **Backwards Compatibility**: All legacy input files supported

Run tests with CTest:
```bash
ctest -L regtest
```

Run Phase 5 tests:
```bash
cd regtest && python3 run_phase5_tests.py
```

## Build Options

Customize the build by passing variables to CMake:

* `-DMASSCONSISTENT_GPU_BACKEND=[NONE|CUDA|HIP|SYCL]` — Enable GPU acceleration (default: `NONE`)
* `-DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=[ON|OFF]` — Build Python API wrapper (default: `OFF`)
* `-DMASSCONSISTENT_ENABLE_MPI=[ON|OFF]` — Enable MPI multi-node parallelism (default: `OFF`)
* `-DMASSCONSISTENT_USE_VENDORED_AMREX=[ON|OFF]` — Use vendored AMReX (default: `ON`)

## Advanced Capabilities

- **Agricultural Drone Operations**: Simulate drone spray drift, rotor downwash, and canopy deposition
- **Wildfire Coupling**: One-way coupling with fire propagation models
- **External Integrations**: FLORIS, PyWake, WAsP wind farm tools; PHREEQC geochemistry
- **Python API**: Integrate with external models and workflows
