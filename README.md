# massconsistent_amr

[![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml)
[![Build and Deploy Documentation](https://github.com/hgopalan/massconsistent_amr/actions/workflows/docs.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/docs.yml)

A terrain-following, mass-consistent 3-D wind solver built on [AMReX](https://amrex-codes.github.io/amrex/).

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
- **Gaussian puff dispersion** — Passive pollutant transport
- **Python API** — Coupling with fire and atmospheric models
- **GPU-ready** — Runs on NVIDIA, AMD, and Intel GPUs via AMReX

## Documentation

See the [full documentation](https://hgopalan.github.io/massconsistent_amr/) for:

- Building and installation instructions
- Input parameter reference
- Usage examples and tutorials
- Wind initialization modes (log-law, uniform, RAWS, surface_data for HRRR)
- Buildings and wake effects
- Canopy modeling
- Puff dispersion model
- Python API reference
- Regression tests

## License

See [LICENSE](LICENSE).
