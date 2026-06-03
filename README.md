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

## Recent Updates (Phase 4-5)

**Synthetic Turbulence Framework** — Complete three-phase system for terrain-aware wind field generation:
- **Phase 1**: Turbulence parameters (Von Kármán/Kaimal spectra, intensity profiles, coherence functions)
  - ✅ **Parameter Parsing Integrated**: All 13 configuration parameters now parsed from inputs files
  - Spectral models: Von Kármán, Kaimal
  - Intensity profiles: Power-law, Logarithmic, Constant
  - Coherence models: Gaussian, Exponential
- **Phase 2**: Random field synthesis (FFT-based with energy conservation, spatial correlations)
- **Phase 3**: Time-series generation (temporal synthesis for realistic wind fluctuations)

**Phase 4**: Comprehensive validation framework (12 regression tests, all passing)

**OpenFAST Export & Documentation**
- **OpenFAST Export Tool (openfast_export.py)** — Standalone Python tool for exporting wind fields to TurbSim binary format (.bts) compatible with NREL OpenFAST wind turbine simulations
- **BTS Format Writer** — Full TurbSim binary format support with header, metadata, and 3D velocity field export
- **Turbulence Metadata** — Configurable Von Kármán spectrum, intensity profiles, integral length scales, and surface roughness
- **Regression Tests** — Validation with Gaussian hill test case and comprehensive format compliance tests

**Synthetic Turbulence Example Usage**

Create an inputs file with synthetic turbulence enabled:

```ini
# Enable synthetic turbulence generation
enable_synthetic_turbulence = true

# Phase 1: Turbulence Parameters
turbulence_spectrum_model = VonKarman          # or Kaimal
turbulence_intensity_model = PowerLaw          # or Logarithmic, Constant
turbulence_coherence_model = Gaussian          # or Exponential
turbulence_intensity_ref = 0.12                # turbulence intensity [fraction]
turbulence_z_intensity_ref = 10.0              # reference height [m]
turbulence_intensity_exponent = 0.14           # power-law exponent
turbulence_length_scale_u = 300.0              # u-component length scale [m]
turbulence_length_scale_v = 200.0              # v-component length scale [m]
turbulence_length_scale_w = 120.0              # w-component length scale [m]
turbulence_coherence_decay_vertical = 0.008    # vertical coherence decay [1/m]
turbulence_coherence_decay_lateral = 0.006     # lateral coherence decay [1/m]
turbulence_anisotropy_ratio_v = 0.80           # v/u velocity ratio
turbulence_anisotropy_ratio_w = 0.50           # w/u velocity ratio

# Phase 2: Random Field Generation
turbulence_random_seed = 12345                 # reproducible random fields

# Phase 3: Export
turbulence_export_format = bts                 # TurbSim binary format
turbulence_output_file = turbulence.bts        # output filename
```

Then run:
```bash
./build/wind_solver your_inputs.i
# Output will include: turbulence.bts + optional turbulence.bts.meta
```

See [Validation & Optimization documentation](https://hgopalan.github.io/massconsistent_amr/validation_optimization.html) for detailed parameter sensitivity methodology and [Advanced Solver Features](https://hgopalan.github.io/massconsistent_amr/advanced_solver_features.html) for synthetic turbulence framework, and [Tools README](tools/README.md) for tool usage examples.

## Advanced Solver Capabilities

The solver includes three implementation layers for advanced features:

- **Foundation Layer** — Comprehensive parameter documentation and modular header files enabling configuration and regression testing
- **Integration Layer** — Kernel integration into main simulation loops with output field diagnostics
- **Validation & Optimization Layer** — Performance profiling on CPU/GPU, physical correctness validation, parameter sensitivity analysis, and production hardening

See [Validation & Optimization documentation](https://hgopalan.github.io/massconsistent_amr/validation_optimization.html) for detailed validation framework and performance profiling tools.

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
- Advanced physics features (boundary layer diagnostics, diurnal roughness, ageostrophic balance)
- FLORIS wind farm integration
- Python API reference
- Regression tests and implementation status

## License

See [LICENSE](LICENSE).
