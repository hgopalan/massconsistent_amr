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

## Wind Field Initialization Modes

The solver supports three initialization modes via the `init_mode` parameter:

### 1. Log-Law (`init_mode = loglaw`, default)
Uses a logarithmic wind profile above terrain, scaled from a reference wind speed at a reference height.
```
U_ref  = 10.0        # x-component of reference wind [m/s]
V_ref  = 0.0         # y-component of reference wind [m/s]
z_ref  = 10.0        # reference height above local terrain [m]
z0     = 0.1         # aerodynamic roughness length [m]
```

### 2. Uniform (`init_mode = uniform`)
Uses a constant wind field everywhere above terrain (independent of height).
```
uniform_U = 8.5      # uniform x-component [m/s]
uniform_V = 2.0      # uniform y-component [m/s]
```

### 3. RAWS (`init_mode = raws`)
Interpolates wind from a velocity file using inverse-distance weighting (IDW).
The velocity file should have columns: `X Y Z Ux Uy` (coordinates in m, components in m/s).
```
velocity_file = velocity.csv   # path to wind station data
```

A Python tool `tools/farsite_weather_reader.py` is provided to convert FARSITE weather (.wtr) files to the required velocity CSV format.

See [LICENSE](LICENSE).
