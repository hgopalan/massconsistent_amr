# massconsistent_amr

**massconsistent_amr** is a high-performance, GPU-accelerated (CUDA/HIP/SYCL), and MPI-parallel C++ 3-D mass-consistent wind diagnostic solver built on the AMReX framework. It features advanced terrain-following adjustment with spatially-varying anisotropy, building/canopy drag, analytical turbine wake modeling, directional bias correction, Coriolis parameter scaling, and advanced atmospheric dispersion (Lagrangian Puff and LPDM) with simple reactive chemistry. It also integrates with external tools via Python, supporting wind farm utilities (FLORIS, PyWake), wildfire modeling, and geochemical reactive transport (PHREEQC).

## Scenario Gallery

The solver supports the following eight core scenarios:

<table width="100%">
  <tr>
    <td width="50%" valign="top">
      <h4>1. Complex Terrain-Following Coordinate Flow</h4>
      <p>Horizontal wind speed distribution above complex terrain at 50 m above ground level with terrain-following coordinate transformation.</p>
      <img src="docs/terrain_following_complex_flow.png" alt="Complex Terrain-Following Flow" width="100%"/>
    </td>
    <td width="50%" valign="top">
      <h4>2. Gorge Bridge Crossing Wind Loading</h4>
      <p>Bridges crossing deep gorges experience funneling speedup, vertical wind shear, and vortex-induced cable resonance.</p>
      <img src="docs/gorge_bridge_crossing.png" alt="Gorge Bridge Crossing" width="100%"/>
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <h4>3. Urban Street Canyon and Building Wakes</h4>
      <p>Wind speed distribution at 15 m above ground level in urban layouts with complex building geometries (box, L-shaped, T-shaped, U-shaped, polygonal) showing wake effects and drag parameterizations.</p>
      <img src="docs/urban_street_canyon.png" alt="Urban Street Canyon" width="100%"/>
    </td>
    <td width="50%" valign="top">
      <h4>4. Transmission Tower and Line Wind Loading</h4>
      <p>Structural wind loading, catenary line tension, and sway displacement across complex ridges. Brighter colors indicate high-speed gap-flow winds with elevated wind-drag loading, while darker colors represent lower wind speed regions.</p>
      <img src="docs/transmission_line_loading.png" alt="Transmission Line Loading" width="100%"/>
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <h4>5. Yawed Wind Turbine Wake Deflection</h4>
      <p>Analytical wake deficit showing lateral center-line deflection under yawed operation using Bastankhah model with turbine rotor orientation.</p>
      <img src="docs/turbine_wake_deflection.png" alt="Turbine Wake Deflection" width="100%"/>
    </td>
    <td width="50%" valign="top">
      <h4>6. Geochemical Hotspot and O₂ Delivery Detection</h4>
      <p>Valley AMD discharge point risk assessment based on wind-speed-dependent Sherwood mass transfer correlations.</p>
      <img src="docs/valley_amd_hotspots.png" alt="Valley AMD Hotspots" width="100%"/>
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <h4>7. Agricultural Drone Spray Drift and Deposition</h4>
      <p>Drone pesticide application modeling with spray drift, dynamic canopy deposition, and rotor downwash velocity mapping over complex terrain.</p>
      <img src="docs/drone_deposition_plot.png" alt="Agricultural Drone Spray Drift" width="100%"/>
    </td>
    <td width="50%" valign="top">
      <h4>8. 3D Puff and Particle Dispersion Modeling</h4>
      <p>Continuous and puff source release tracking over complex terrain with Pasquill-Gifford atmospheric stability classes, wet/dry deposition, and boundary-layer reflection.</p>
      <img src="docs/puff_deposition_plot.png" alt="Atmospheric Puff and Particle Dispersion" width="100%"/>
    </td>
  </tr>
</table>

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

[Full documentation](https://hgopalan.github.io/massconsistent_amr/)

## Environment Setup

The project includes conda environment files for easy dependency management:

* **`environment.yml`** — Minimal environment for Python tools and utilities
* **`environment-dev.yml`** — Development environment with C++ build tools (recommended for building)
* **`environment-full.yml`** — Complete environment with all optional packages (FLORIS, PyWake, etc.)

### Create Python Environment

```bash
# Minimal (Python tools only)
conda env create -f environment.yml
conda activate massconsistent_amr

# Development (with C++ compilers)
conda env create -f environment-dev.yml
conda activate massconsistent_amr-dev

# Full suite (all optional packages included)
conda env create -f environment-full.yml
conda activate massconsistent_amr-full
```

See [INSTALL.md](INSTALL.md) for detailed setup instructions, troubleshooting, and platform-specific guidance.

## Quick Start

### Python Tools Only (No Compilation)

```bash
git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
cd massconsistent_amr
conda env create -f environment.yml
conda activate massconsistent_amr
# Now use Python tools
```

### Build from Source (Requires C++ Tools)

```bash
git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
cd massconsistent_amr

# Create development environment
conda env create -f environment-dev.yml
conda activate massconsistent_amr-dev

# Build the solver
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_USE_VENDORED_AMREX=ON
cmake --build build --parallel

# Run regression test
./build/wind_solver regtest/gaussian_hill/inputs.i
```

## Build Options

Customize the build by passing variables to CMake:

* `-DMASSCONSISTENT_GPU_BACKEND=[NONE|CUDA|HIP|SYCL]` — Enable GPU acceleration (default: `NONE`)
* `-DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=[ON|OFF]` — Build Python API wrapper (default: `OFF`)
* `-DMASSCONSISTENT_ENABLE_MPI=[ON|OFF]` — Enable MPI multi-node parallelism (default: `OFF`)
* `-DMASSCONSISTENT_USE_VENDORED_AMREX=[ON|OFF]` — Use vendored or external AMReX (default: `ON`). For external, set `-DAMReX_DIR=/path/to/amrex`.

Example: `cmake -S . -B build -DMASSCONSISTENT_GPU_BACKEND=CUDA -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON`

### Python-Only Installation (Without Compilation)

For users who want to use Python tools without building the C++ solver:

```bash
# Using pip (requires manual CMake installation)
pip install -r requirements.txt

# For optional packages (FLORIS, PyWake, etc.)
pip install -r requirements-optional.txt

# Or use conda
conda env create -f environment.yml
```

See [INSTALL.md](INSTALL.md) for detailed instructions on all installation methods.

## Features

### 1. Mass-Consistent Solver and Wake Modeling
- **Mass-Consistent Wind Field:** Enforces $\nabla \cdot \mathbf{u} = 0$ over complex terrain using a variational Lagrange multiplier approach.
- **Spatially-Varying Anisotropy:** Adapts adjustment coefficients ($\alpha_h / \alpha_v$) cell-locally based on slope, Richardson number, and Froude number.
- **Topographic Barrier Shielding:** Penalizes interpolation weights across high mountain ridges to prevent unphysical station influence in adjacent valleys.
- **Physical Parameterizations:** Integrates stable/unstable Monin-Obukhov profiles, orographic speed-up, katabatic/anabatic slope flows, sea breeze, and valley channeling.
- **Meteorological Ingestion:** Supports 3D NetCDF NWP inputs (WRF, GFS) with anisotropic Inverse Distance Weighting to preserve vertical profiles.
- **MacDonald Canopy Drag:** Models vegetative canopy drag with exponential wind decay within canopy height ($h_c$) and displacement height corrections.
- **Building Wake Models:** Incorporates Röckle, Huber-Snyder, and AERMOD PRIME building downwash parameterizations with support for rectangular, cylindrical, and pitched-roof geometries. Polygon support enables modeling of complex urban shapes (L/T/U-shaped buildings) and internal courtyards. Wake models include: far-wake extension to 15H, oblique angle cavity scaling, tall-building corrections, Gaussian lateral profiles, upwind recirculation zones, corner acceleration effects, horseshoe vortex modeling, two-layer height-dependent deficit decay, entrainment-based far-wake recovery, pedestrian wind comfort assessment, aspect-ratio dependent cavity correction, and urban canyon attenuation (see [Building Wake Enhancements](docs/mathematical_models.rst#advanced-building-wake-model-enhancements) for details).
- **AMReX Embedded Boundary (EB) Support:** Represents arbitrary 3D shapes (boxes, cylinders, spheres, STL geometries) using AMReX EB2 with fluid volume fraction marking.
- **Analytical Turbine Wakes:** Supports Jensen, Bastankhah Gaussian, TurbOPark, and Gauss-Curl Hybrid wake models with wake centerline deflection, yaw, and AEP calculation.
- **Wake Superposition and Added Turbulence:** Computes overlapping deficits (quadratic/linear/geometric) and wake-added turbulence (Crespo-Hernández, Frandsen) with convective buoyant wake destruction.

### 2. 3D Scalar Transport and Mixing
- **3D Advection-Diffusion:** Solves transport of temperature and moisture using conservative upwind advection and mixing-length eddy diffusivity.
- **1D Mixing Solver:** Simulates vertical surface layer transitions and boundary layer mixing.
- **Spatially-Varying Boundary Layer Height:** Diagnoses boundary layer height ($h_{pbl}$) using column-scanning bulk Richardson number profiles.
- **Sky View Factor & Solar Shading:** Computes sky view factor from combined terrain+building elevation field and solar shading based on sun position, enabling radiation-dependent thermal effects and urban canyon heating.

### 3. Dispersion Model
- **Gaussian Puff Dispersion:** Tracks 3D Gaussian puffs with Pasquill-Gifford stability, Briggs plume rise, gravitational settling, dry deposition, and precipitation scavenging.
- **Lagrangian Particle LPDM:** Simulates stochastic particle trajectories with Wiener processes and vertical drift correction to prevent spurious accumulation.
- **Dense Gas Dispersion (SLAB/UGC):** Models hazardous material releases with density-ratio tracking, Froude number regime detection, gravity-driven spreading, and SLAB layer height decay (CO₂, HF, Cl₂, NH₃ emissions).
- **Simple Reactive Chemistry:** First-order exponential decay of NO₂, SO₂, HCl, and NH₃ with stoichiometric product formation (AERMOD TOXICS level). Supports seasonal and temperature corrections.
- **Ammonia Gas-Liquid Exchange:** Specialized model for ammonia over water with temperature-dependent Henry's law constant, two-film theory mass transfer, and salinity corrections.

### 4. Synthetic Fluctuations (Turbulence)
- **Terrain-Aware Masking:** Confines synthetic turbulent fluctuations to fluid regions and blends smoothly near terrain boundaries.
- **Spectral Turbulence Models:** Generates fluctuations using Kaimal or Von Kármán spectra with height-varying intensity and coherence, plus Mann 3D anisotropic turbulence box modeling.
- **Downstream Export:** Exports OpenFAST/TurbSim compatible binary (.bts) formats.

### 5. Infrastructure Vulnerability Assessment
- **Bridge Loading:** Computes vertical/lateral drag forces, ISO-comfort human accelerations, and vortex-induced resonant shedding frequencies.
- **Transmission Line Assessment (IEEE 738):** Solves conductor heat balance to calculate dynamic line ratings, sag, and wind drag across complex terrain.
- **Structure Loading:** Evaluates static base shear, dynamic amplification (gust response), lateral bending deflection, and structural fragility curves.

### 6. Wind Farm Optimization & Turbine Wake Modeling
- **Wind Farm Integration:** Exports resolved wind fields and wake-adjusted velocity distributions to external wind farm optimization tools.
- **FLORIS Integration:** Full compatibility with NREL's FLORIS wind farm optimization framework for wake steering, yaw control, and power maximization.
- **PyWake Integration:** Supports PyWake for advanced farm-level wake analysis and optimization workflows.
- **WAsP Export:** Exports wind resource data in WAsP-compatible formats for wind resource assessment.
- **Turbine Wake Optimization:** Leverages analytical turbine wake models (Jensen, Bastankhah, TurbOPark) for layout optimization and wake steering calculations.

### 7. Wildfire Levelset Coupling
- **Fire Front Propagation:** One-way coupling with fire propagation models using sensible heat flux feedback to enable wind-fire interactions.
- **Heat-Driven Flow:** Models buoyancy-driven flows induced by wildfire heat release and their impact on wind field modification.
- **Integration:** Seamless coupling with wildfire_levelset repository for comprehensive wildfire-wind modeling scenarios.

### 8. Atmospheric Processes & Meteorological Effects
- **Coriolis Parameter Scaling:** Latitude-dependent Coriolis force modeling to account for Earth's rotation effects on flow deflection.
- **Directional Bias Correction:** Systematic correction of wind direction and speed biases from NWP model inputs, including terrain-induced channeling bias.
- **Ageostrophic Wind Adjustment:** Non-geostrophic force effects including pressure-gradient and surface-friction driven flow adjustments.
- **Thermal Circulation:** Diurnal heating-driven slope flows, sea breeze, and valley circulation modeling.
- **Buoyancy-Driven Flows:** Vertical motion suppression and horizontal deflection under stable stratification with Froude number regime detection.
- **Surface Layer Transition:** Smooth blending between log-law profiles at low heights and mixed-layer profiles at boundary-layer top.
- **Diurnal Roughness Variations:** Time-varying aerodynamic roughness length to capture diurnal surface heating effects on roughness.

### 9. Advanced Terrain & Blocking Effects
- **Terrain Blocking Models:** Flow blockage and deflection around mountain barriers based on Froude number and terrain steepness.
- **Roughness Transitions:** Spatial transitions between different land-use categories with smooth interpolation to prevent discontinuities.
- **Roughness Blocking Method:** Representation of sub-grid scale roughness features through explicit blocking approach.
- **Porosity Models:** Porous media flow treatment for vegetated and built environments.
- **Valley Channeling:** Enhanced wind speed in valleys due to topographic funneling with anisotropic adjustment.
- **Gap Flow Models:** Acceleration through mountain passes and gorges with pressure-driven channeling.
- **Morphometric Analysis:** Terrain curvature, slope aspect, and landform classification for parameterization adaptation.

### 10. Infrastructure & Structural Assessment (Enhanced)
- **Transmission Wire Dynamics:** Conductor sag, tension, and galloping/wake-induced oscillations under wind loading.
- **Structure Resonance:** Dynamic amplification factors, gust response factors, and structural fragility curve evaluation.
- **Pedestrian Wind Comfort:** ISO standard (ISO 23601) wind comfort assessment in urban environments.

### 11. Advanced Data Processing & Validation
- **Continuity Validation:** Field-level divergence checking and diagnostics to ensure mass conservation.
- **Flux Diagnostics:** Mass flux, sensible heat flux, and momentum flux calculations across domain sections.
- **Numerical Derivatives:** High-order derivative operators (central, WENO-3, WENO-5) for accurate gradient computation.
- **Numerical Optimization:** Iterative refinement of wind field parameters for improved fit to observations.

### 12. Single Column Model (SCM) Wind Profile Initialization
- **Wind Speed Specification:** Direct initialization from a specified wind speed at a reference height (e.g., meteorological mast) without log-law assumptions.
- **Geostrophic Wind Recursion:** Iteratively computes geostrophic wind components required to produce target wind speed at reference height through time-dependent 1D simulation.
- **Physics-Based Profile Evolution:** Solves coupled momentum, temperature, and turbulent kinetic energy equations with Coriolis forcing, turbulent diffusion, and stratification effects.
- **Terrain-Aligned Initialization:** Maps converged 1D profiles to 3D domain with proper terrain alignment. See [SCM Initialization Documentation](docs/scm_initialization.rst) for detailed methodology.
- **Temperature Profile Generation:** Initializes temperature field using user-defined reference temperature and lapse rate, enabling proper atmospheric stratification.
- **1D Solver Efficiency:** Completes pre-processing in seconds to minutes before 3D solve, with no additional cost to main solver.

### 13. Agricultural Drone Operations
- **Flight Path Modeling:** Parses and interpolates 3D flight trajectories from CSV telemetry data for agricultural drone operations.
- **Rotor Downwash Velocity:** Models 3D analytical rotor downwash velocity fields with jet expansion and forward flight deflection effects.
- **Spray Drift Simulation:** Simulates spray drift and canopy deposition using Lagrangian Particle Dispersion Models (LPDM) or Gaussian Puff dispersion.
- **Field Application:** Enables precise pesticide and nutrient application modeling over complex terrain with wind speed-dependent drift prediction.

### 14. Operational Wind Farm Management
- **Power Curtailment and Control:** Real-time farm-level and per-turbine power limiting with ramp rate constraints, enabling grid compliance and curtailment revenue optimization.
- **SCADA Data Integration:** Ingests real-time turbine telemetry (wind speed, power output, yaw, pitch, ambient conditions) with automated quality validation and time-series buffering.
- **Wind Speed and AEP Forecasting:** Persistence-based 10-minute ahead forecasting with uncertainty quantification for grid dispatch planning.
- **Wake Loss Diagnostics:** Per-turbine wake loss analysis with upwind relationship identification, enabling rapid performance troubleshooting.
- **Enhanced AEP Accounting:** Availability factors, seasonal energy breakdown, and micro-siting analysis for investor reporting and contractual guarantee validation.


## Test Cases

Test cases are located in `test/` and documented in `test/README.md`.

## Regression Tests

Over 80 automated regression tests are located in `regtest/` covering the core solver, wake models, turbulence, dispersion, SCM (Single Column Model) boundary layer schemes (including YSU, MYJ, and microphysics), and wildfire coupling.

Run them with CTest from your build directory:
```bash
ctest -L regtest
```

## External Coupling

The solver integrates with external models for wind farm optimization, geochemical analysis, and fire modeling. For guides, API references, and examples, see the [External Coupling Documentation](docs/external_coupling.rst) and the [wildfire_levelset](https://github.com/hgopalan/wildfire_levelset) repository.

## License

See [LICENSE](LICENSE).
