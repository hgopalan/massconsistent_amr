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

See the [documentation](https://hgopalan.github.io/massconsistent_amr/) for full details on building options, input parameters, output files, and regression tests.

## License

See [LICENSE](LICENSE).
