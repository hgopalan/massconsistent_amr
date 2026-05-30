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

## Buildings Support

Buildings can be specified in a CSV file with the format:
```
building_file = buildings.csv
```

The CSV file should contain one building per line with columns: `xmin xmax ymin ymax zmin zmax` (all in metres):
```
# xmin  xmax  ymin  ymax  zmin  zmax [m]
40.0    60.0  40.0  60.0  0.0   30.0
100.0   140.0 60.0  80.0  0.0   50.0
```

Cells inside buildings are masked (zero velocity) similar to terrain cells. The vertical domain automatically extends to accommodate the tallest building.

### Building Wake Effects

The solver implements the Röckle (1990) wake parameterization to model velocity deficits behind buildings. Enable wake modeling with:
```
enable_wake = true
wake_c1 = 0.9                    # Cavity length coefficient (Lr = c1 * H)
wake_c2 = 0.3                    # Wake deficit coefficient  
wake_separation_length = 3.0     # Wake extends to 3*H downwind
```

### Multiple Building Support

For multiple buildings, the solver supports advanced wake modeling:

**Wake Superposition** (default: enabled): When multiple building wakes overlap, velocity deficits are combined using quadratic superposition for physically realistic turbulent mixing:
```
wake_superposition = true        # Use quadratic superposition (recommended)
wake_superposition = false       # Use linear addition (legacy behavior)
```

**Street Canyon Effects**: Models flow in street canyons (parallel building rows) using the Oke (1988) classification. Flow regime depends on height-to-width ratio (H/W):
- H/W < 0.3: Isolated roughness (minimal interaction)
- 0.3 < H/W < 0.7: Wake interference flow
- H/W > 0.7: Skimming flow (vortex in canyon)

Enable street canyon modeling:
```
enable_street_canyon = true
street_canyon_reduction = 0.3    # Velocity reduction factor (0-1)
```

See `regtest/building_array/` for a complete example with a 3×3 building array demonstrating wake superposition and street canyon effects.

###  Rooftop Vortices and Building Orientations:**

For improved physical realism and support of non-grid-aligned buildings:

**Rooftop Vortices**: The cavity zone now includes vertical circulation due to rooftop vortex formation. This adds realistic vertical velocity components to the wake model, improving prediction of wind patterns in the immediate vicinity of buildings.

**Building Orientation**: Buildings can now be rotated to arbitrary angles (Phase 3 feature). The 7th column in the buildings CSV file specifies the rotation angle in degrees:
```
# Buildings CSV: xmin xmax ymin ymax zmin zmax [rotation_degrees]
100.0 150.0 200.0 250.0 0.0 30.0       # Grid-aligned (default: 0°)
300.0 350.0 350.0 400.0 0.0 25.0 45.0  # Rotated 45° counter-clockwise
```

For oriented buildings, the effective width and length are computed by projecting the rotated geometry onto the wind direction, ensuring accurate wake modeling regardless of building alignment.

See `regtest/rooftop_vortex/` for rooftop vortex validation and `regtest/building_oriented/` for orientation effects.

## Python API

The solver can be controlled from Python for coupled wind-fire simulations. Build with Python bindings enabled:

```bash
cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build --parallel
export PYTHONPATH=$PWD/build/python:$PYTHONPATH
```

### Quick Python Example

```python
from wind_solver import WindSolver

# Initialize and solve
wind = WindSolver("inputs.i")
wind.solve()

# Extract velocity at 10m above ground level
vel_agl = wind.get_velocity_at_agl(10.0)
print(f"Mean wind at 10m: U={vel_agl['u'].mean():.2f} m/s")

# Write output
wind.write_plotfile("plt_wind")
wind.finalize()
```

### Coupled Wind-Fire Simulations

The Python API enables coupling with external fire solvers like [wildfire_levelset](https://github.com/hgopalan/wildfire_levelset):

```python
from wind_solver import WindSolver
from wildfire_solver import WildfireSolver  # from wildfire_levelset

wind = WindSolver("wind_inputs.i")
fire = WildfireSolver("fire_inputs.i")

# Solve wind and pass to fire solver
wind.solve()
vel_3d = wind.get_velocity()
fire.update_wind_3d(vel_3d['u'], vel_3d['v'], vel_3d['w'],
                    wind.nz, wind.zmin, wind.zmax)

# Run coupled simulation
for n in range(num_steps):
    fire.step()

wind.finalize()
fire.finalize()
```

See [docs/features/python_api.md](docs/features/python_api.md) for complete API documentation, examples, and coupling workflows.

## License

See [LICENSE](LICENSE).
