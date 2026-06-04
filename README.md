# massconsistent_amr

An AMReX-based C++ mass-consistent 3-D wind diagnostic solver providing terrain-following wind field adjustment with GPU-ready kernels (CUDA/HIP/SYCL), building wake parameterization, canopy effects, and optional MPI parallelism.

## CI / Build Status

| Configuration | Status |
|---------------|--------|
| Linux & macOS CPU (GCC/Clang, Release + Debug) | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Windows CPU (MSVC, Release + Debug) | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build_windows)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Linux GPU — CUDA 12.6 | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build_cuda)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Windows GPU — CUDA 12.6 ⚠️ | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build_windows_cuda)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Linux GPU — HIP/ROCm 6.2 | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build_hip)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Linux GPU — SYCL/oneAPI 2025.x | [![CMake Build](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml/badge.svg?job=build_sycl)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/cmake_build.yml) |
| Documentation | [![Build and Deploy Documentation](https://github.com/hgopalan/massconsistent_amr/actions/workflows/docs.yml/badge.svg)](https://github.com/hgopalan/massconsistent_amr/actions/workflows/docs.yml) |

⚠️ **Note:** Windows CUDA build is non-blocking (experimental); failures do not affect overall CI status.

📖 **[Full documentation](https://hgopalan.github.io/massconsistent_amr/)**

## Quick Start

```bash
git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
cd massconsistent_amr
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/wind_solver regtest/gaussian_hill/inputs.i
```

## Build Options

The CMake build system supports multiple configuration options for customizing the build:

### Basic Configuration

```bash
# CPU-only Release build (default)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# CPU-only Debug build (with optimizations disabled for debugging)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

### GPU Acceleration

Enable GPU support by specifying the backend:

```bash
# NVIDIA CUDA (requires CUDA toolkit 12.0+)
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_GPU_BACKEND=CUDA
cmake --build build --parallel

# AMD HIP/ROCm (requires ROCm 6.0+)
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_GPU_BACKEND=HIP
cmake --build build --parallel

# Intel SYCL/oneAPI (requires oneAPI 2024.0+)
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_GPU_BACKEND=SYCL
cmake --build build --parallel
```

### Python Bindings

Build Python bindings for integration with fire simulation and atmospheric models:

```bash
# Enable Python bindings (requires pybind11 and Python 3.6+)
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build --parallel

# After building, the Python module is available at:
# build/src/python/pyWindSolver.*.so
# Can be imported as: from src.python import pyWindSolver

# Or install to Python site-packages:
pip install -e .
```

### MPI Parallelism

Enable distributed memory parallelism for large-scale simulations:

```bash
# Enable MPI support (requires MPI implementation like OpenMPI or MPICH)
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_ENABLE_MPI=ON
cmake --build build --parallel

# Run with MPI:
mpirun -np 4 ./build/wind_solver regtest/gaussian_hill/inputs.i
```

### Combined Options

Example: GPU (CUDA) + Python + MPI:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_GPU_BACKEND=CUDA \
  -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON \
  -DMASSCONSISTENT_ENABLE_MPI=ON
cmake --build build --parallel
```

### Documentation Build

Build Sphinx documentation (requires Sphinx and Python):

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_BUILD_DOCS=ON
cmake --build build --target docs
# Documentation will be in: build/docs/_build/html/
```

### Advanced AMReX Options

For custom AMReX builds (e.g., external installation):

```bash
# Use system-installed AMReX instead of vendored submodule
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_USE_VENDORED_AMREX=OFF \
  -DAMReX_DIR=/path/to/amrex/lib/cmake/AMReX
cmake --build build --parallel
```

## Features

- **Mass-consistent wind solver** — Enforces ∇·u = 0 using Lagrange multiplier approach
- **Terrain-following** — Log-law wind profiles over complex topography
- **Multiple initialization modes** — Log-law, uniform, RAWS stations, HRRR-style surface parameters, or power-law profiles
- **Position-dependent roughness** — Spatially-varying aerodynamic roughness length z₀ from file or land-use classification
- **Building support** — Wake modeling with Röckle (1990), Huber-Snyder (EPA), and AERMOD PRIME (EPA regulatory) parameterizations; adaptive wake superposition with distance-weighted blending
- **Canopy modeling** — Forest canopy drag effects
- **Height-dependent anisotropy** — Vertical adjustment coefficient α_v can vary with height
- **Non-neutral stability corrections** — Businger-Dyer Monin-Obukhov profiles for stable/unstable conditions; adaptive stability model selection based on bulk Richardson number (Ri_b)
- **Elevation-dependent wind scaling** — Terrain elevation effects on reference wind speed for mountain-valley flows
- **Orographic speed-up** — Jackson & Hunt (1975) model for hill/ridge acceleration and lee-side flow separation; adaptive activation with Froude number and slope thresholds
- **Sea breeze parameterization** — Thermal circulation from land-sea temperature contrast driving onshore/offshore winds
- **Froude number terrain blocking** — Flow blocking and channeling around steep terrain in stable stratification
- **Katabatic/Anabatic slope flows** — Thermally-driven up-slope (daytime) and down-slope (nighttime) flows on inclined terrain
- **Valley channeling factor** — Wind alignment with valley axis and speed adjustment based on valley geometry (width/depth)
- **Gap flow parameterization** — Pressure-driven channeling through mountain gaps/passes with 2-4× wind speed enhancement
- **Time-varying boundary conditions** — Support for transient wind simulations with time series input
- **Building porosity model** — Porous flow through structures (trees, fences) with drag parameterization
- **Thermal stratification with buoyancy** — Temperature-driven buoyancy effects on vertical velocity using Boussinesq approximation
- **Kinematic terrain-following BC** — No-flow-through condition w = u·∇h at terrain surface for improved terrain representation
- **Tunable multigrid solver** — Configurable MLMG parameters and bottom solver selection for performance optimization
- **Surface Flux Diagnostics** — Computes friction velocity, momentum flux, drag coefficient, sensible/latent heat flux
- **Land-use Roughness Classification** — Categorical z₀ mapping from NLCD/IGBP land-use categories
- **Directional Bias Correction** — Corrects systematic wind direction and speed biases from NWP models
- **3D Meteorological Ingestion (NetCDF)** — Horizontal, terrain-aware vertical, and temporal interpolation of 3D NWP model outputs (e.g. WRF, GFS) into the solver grid
- **Solver enhancements** — Divergence damping filter, optional perturbation pressure gradient, multi-scale terrain analysis, smooth boundary layer transition
- **Performance timing** — Detailed timing output for profiling and optimization
- **Terrain-following coordinates** — Streamline coordinate transformation for improved accuracy on steep terrain
- **Gaussian puff dispersion** — Passive pollutant transport with enhanced physics:
  - **Height-dependent diffusivity** — K(z) profiles for realistic atmospheric mixing
  - **First-order decay** — Exponential decay for radioactive/chemical species
  - **Plume rise** — Briggs buoyancy formula for heated sources (WindNinja/QUIC-PLUME compatible)
- **Enhanced diagnostics** — Surface heat flux and drag coefficient output fields, momentum flux, boundary layer depth
- **Advanced physics features** — Simplified Richardson number stability classification, roughness blocking from buildings, latitude-dependent Coriolis parameter, power-law wind profile option above boundary layer, enhanced heat flux diagnostics
- **Python API** — Coupling with fire and atmospheric models
- **FLORIS integration** — Export wind data to FLORIS wind farm simulation format
- **GPU-ready** — Runs on NVIDIA, AMD, and Intel GPUs via AMReX

## Synthetic Turbulence

The solver synthesizes **terrain-aware turbulent fluctuations** using the following approach:

### Overview

The synthetic turbulence system generates time-resolved velocity fields with proper handling of terrain boundaries. Key capabilities include:

- **Terrain-aware masking** — Fluctuations are confined to the fluid region (z_agl > 0)
- **Smooth blending** — Cosine ramp transition from terrain surface to free field
- **Mass conservation** — Masking preserves divergence-free property of base field
- **Spectral models** — Von Kármán or Kaimal spectrum options
- **Height-dependent intensity** — Power-law or logarithmic intensity profiles
- **Time-resolved fields** — Generate 3D velocity fluctuations over time steps
- **BTS export** — OpenFAST/TurbSim compatible binary format
- **Statistical validation** — Built-in validation against spectral targets

### Terrain-Aware Fluctuation Algorithm

Fluctuations are masked using a smooth terrain mask `mask(x, y, z)`:

```
mask(z_agl) = {
    0.0,                              if z_agl ≤ 0
    (1 - cos(π·z_agl/h_t))/2,       if 0 < z_agl < h_t
    1.0,                              if z_agl ≥ h_t
}
```

where:
- `z_agl = z_physical - z_terrain(i,j)` is height above ground level
- `h_t = 2–4 m` is the transition height (typically 2–3 grid cells)

**Key Properties**:
1. **Zero inside terrain** — No unphysical fluctuations penetrate solid ground
2. **Smooth transition** — C¹ continuous derivative at boundaries (cosine ramp)
3. **Mass conservation** — Base field remains divergence-free; masking error is bounded and smooth
4. **Physical realism** — Matches atmospheric boundary layer behavior

### Mass Conservation Details

The base velocity field (from mass-consistent solver) satisfies ∇·u_base = 0 exactly. When masked fluctuations are applied:

```
u_modified = u_base + (u_fluct * mask)
```

The modified field maintains approximate mass conservation because:

1. Base field: ∇·u_base = 0 (exactly divergence-free)
2. Masked fluctuations: ∇·(α·u_fluct) = α·∇·u_fluct + u_fluct·∇α
3. The gradient term is bounded: |∇α| ~ 0.2–0.4 m⁻¹
4. Error is O(dz), minimized by smooth cosine masking
5. Error is zero at domain boundaries (where mask = constant)

Optional post-processing with divergence damping (available in `src/divergence_damping.H`) can enforce strict mass conservation if needed.

### Configuration

Enable synthetic turbulence in `inputs.i`:

```
wind_solver.enable_synthetic_turbulence = 1

# Spectrum model (VonKarman or Kaimal)
wind_solver.turbulence_spectrum_model = VonKarman

# Intensity model (PowerLaw, Logarithmic, Constant)
wind_solver.turbulence_intensity_model = PowerLaw
wind_solver.turbulence_intensity_ref = 0.12
wind_solver.turbulence_z_intensity_ref = 10.0
wind_solver.turbulence_intensity_exponent = 0.14

# Length scales [m]
wind_solver.turbulence_length_scale_u = 300.0
wind_solver.turbulence_length_scale_v = 200.0
wind_solver.turbulence_length_scale_w = 120.0

# Anisotropy ratios
wind_solver.turbulence_anisotropy_ratio_v = 0.80
wind_solver.turbulence_anisotropy_ratio_w = 0.50
```

### Python API Usage

```python
from wind_solver import WindSolver

# Initialize and solve
wind = WindSolver("inputs.i")
wind.solve()

# Write with terrain-aware fluctuations
# (automatic masking—no extra parameters needed)
wind.write_plotfile_with_fluctuations("plt_wind_with_fluctuations")

# Output includes:
# ✓ Velocity field with terrain-aligned fluctuations
# ✓ Original vs modified field statistics
# ✓ Fluctuation RMS values (masked and unmasked)
# ✓ Terrain mask diagnostics
```

### Testing

Comprehensive test suites validate the turbulence system. See the [Test Cases](#test-cases) section below.

## Test Cases & Implementation

### Overview

Three comprehensive test cases demonstrate mass-consistent wind solving with time-varying winds, log-law initialization, and synthetic turbulence generation:

1. **Case 1: Gaussian Hill** (Synthetic terrain, ready to run immediately)
2. **Case 2: Flatirons NREL Site** (Real SRTM terrain, Boulder CO)
3. **Case 3: Mt. Hood** (Alpine SRTM terrain, high elevation)

### Tools

#### `tools/gaussian_hill_generator.py`
Generate synthetic Gaussian hill terrain for testing:
- Configurable grid dimensions, domain size, peak elevation
- Adjustable Gaussian width (sigma) parameter
- CSV output compatible with wind solver
- Usage: `python3 gaussian_hill_generator.py --help`

#### `tools/terrain_reader_srtm.py`
Read SRTM DEM data (from wildfire_levelset integration):
- Parse SRTM 1-arcsecond HGT files
- Bilinear interpolation for sub-grid accuracy
- Multi-tile support
- Automatic lat/lon to projected coordinate conversion
- Usage: `python3 terrain_reader_srtm.py N40W105.hgt --output terrain.csv --lat-min 40.010 --lat-max 40.037 --lon-min -105.245 --lon-max -105.218`

### Case 1: Gaussian Hill (Synthetic Terrain)

**Directory**: `test/mass_consistent_case1_gaussian_hill/`

**Terrain**:
- 21×21 grid points over 500×500 m domain
- Gaussian hill with 75 m peak elevation
- Grid spacing: 25 m horizontal

**Features**:
- Log-law initialization (z₀ = 0.05 m)
- Time-varying winds (10 time steps)
- OpenFAST turbulence parameters (Von Kármán spectrum)
- BTS export configuration
- TI = 0.12 baseline turbulence

**Run immediately**:
```bash
cd test/mass_consistent_case1_gaussian_hill
python3 test_case1.py
```

**Validation**:
- Solver initialization
- Wind field solution convergence
- Velocity extraction at 30 m AGL
- Plotfile output generation
- Terrain field access

### Case 2: Flatirons NREL Site (Real Terrain)

**Directory**: `test/mass_consistent_case2_flatirons/`

**Terrain**:
- Real SRTM data (Boulder, CO area)
- ~3.5 km × 3.5 km domain, 21×21 grid
- Rocky foothills with complex topography

**Features**:
- Log-law initialization (z₀ = 0.1 m)
- 20 time-varying wind steps
- TI = 0.14, Von Kármán spectrum
- Wind turbine hub-height extraction (40 m AGL)
- BTS export with 20 time steps

**Setup and run**:
```bash
cd test/mass_consistent_case2_flatirons
# Generate terrain (one-time setup)
python3 ../../tools/terrain_reader_srtm.py N40W105.hgt \
  --output terrain.csv \
  --lat-min 40.010 --lat-max 40.037 \
  --lon-min -105.245 --lon-max -105.218
# Run test
python3 test_case2.py
```

### Case 3: Mt. Hood (Alpine Terrain)

**Directory**: `test/mass_consistent_case3_mt_hood/`

**Terrain**:
- Real SRTM data (Mt. Hood, OR area)
- Summit area ~4 km × 4 km, high elevation
- Alpine terrain with significant relief

**Features**:
- Log-law initialization (z₀ = 0.2 m)
- 25 time-varying wind steps (including gusts)
- Higher TI = 0.16 for complex terrain
- Von Kármán spectrum
- Extraction at 50 m AGL
- BTS export with 25 time steps

**Setup and run**:
```bash
cd test/mass_consistent_case3_mt_hood
# Generate terrain (one-time setup)
python3 ../../tools/terrain_reader_srtm.py N45W121.hgt \
  --output terrain.csv \
  --lat-min 45.366 --lat-max 45.380 \
  --lon-min -121.696 --lon-max -121.680
# Run test
python3 test_case3.py
```

### Output Files Generated

Per test case:
- `plt_case#_winds/` — Corrected wind field (AMReX plotfile)
- `plt_case#_winds_with_fluctuations/` — Wind + turbulence (AMReX plotfile)
- `wind_extract*.csv` — 2D wind field at AGL height
- `case#_turbulence.bts` — Binary BTS file for OpenFAST
- `case#_turbulence.meta` — Metadata file

### Validation Tests

Located in `test/terrain_aware_masking_standalone_test.py`:

```bash
cd /tmp/workspace/hgopalan/massconsistent_amr
python3 test/terrain_aware_masking_standalone_test.py
```

Tests verify (5/5 passing ✓):
1. Terrain mask computation (basic properties)
2. Flat terrain handling
3. No fluctuation penetration into terrain
4. Smooth transition zone blending
5. Mass conservation properties

### Build with Python Bindings

```bash
cmake -S . -B build \
  -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON \
  -DMASSCONSISTENT_ENABLE_OPENFAST=ON
cd build && make -j4
```

### Implementation Files Modified

- `src/python/wind_solver.py` — New methods for terrain-aware masking (97 lines added/modified):
  - `_compute_terrain_mask(terrain)` — Compute 3D terrain mask
  - `write_plotfile_with_fluctuations()` — Apply masked fluctuations
  - `_read_bts_fluctuations()` — Read BTS turbulence files

- Test suite: `test/terrain_aware_masking_standalone_test.py` (358 lines)
  - Comprehensive validation of masking algorithm
  - 5 test cases covering all functional requirements

## Documentation

See the [full documentation](https://hgopalan.github.io/massconsistent_amr/) for:

- Building and installation instructions
- Input parameter reference
- Performance tuning guide
- Usage examples and tutorials
- Synthetic turbulence generation, validation, and BTS/VTK workflows
- Wind initialization modes (log-law, uniform, RAWS, surface_data for HRRR)
- Buildings and wake effects
- Canopy modeling
- Puff dispersion model
- Advanced physics features (boundary layer diagnostics, diurnal roughness, ageostrophic balance)
- FLORIS wind farm integration
- Python API reference
- Regression tests and implementation status

## License

See [LICENSE](LICENSE).
