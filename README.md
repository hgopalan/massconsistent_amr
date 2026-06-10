# massconsistent_amr

An AMReX-based C++ mass-consistent 3-D wind diagnostic solver providing terrain-following wind field adjustment with GPU-ready kernels (CUDA/HIP/SYCL), building wake parameterization (including advanced cavity trapping and plume deformation under wind shear), canopy effects, and optional MPI parallelism.

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
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_USE_VENDORED_AMREX=ON
cmake --build build --parallel
./build/wind_solver regtest/gaussian_hill/inputs.i
```

## Build Options

Customize the build by passing variables to CMake:

* `-DMASSCONSISTENT_GPU_BACKEND=[NONE|CUDA|HIP|SYCL]` — Enable GPU acceleration (default: `NONE`)
* `-DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=[ON|OFF]` — Build Python API wrapper (default: `OFF`)
* `-DMASSCONSISTENT_ENABLE_MPI=[ON|OFF]` — Enable MPI multi-node parallelism (default: `OFF`)
* `-DMASSCONSISTENT_USE_VENDORED_AMREX=[ON|OFF]` — Use vendored or external AMReX (default: `ON`). For external, set `-DAMReX_DIR=/path/to/amrex`.

Example: `cmake -S . -B build -DMASSCONSISTENT_GPU_BACKEND=CUDA -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON`

## Features

- **Mass-consistent wind solver** — Enforces ∇·u = 0 using Lagrange multiplier approach
- **Cell-Local Spatially-Varying Variational Anisotropy** — Formulates and solves the variational wind adjustment problem with a fully 3D spatially-varying anisotropic diagonal weighting tensor A(x, y, z), where local horizontal-to-vertical adjustment coefficients (\alpha_h / \alpha_v) adapt cell-locally based on local terrain slope, local Richardson number, and local Froude number.
- **Chemical/Physical Properties Database Lookup** — ALOHA / Regulatory dictionary lookup of chemical molecular parameters, boiling points, vapor pressures, and AEGL/ERPG/PAC toxicity thresholds by chemical name from JSON database
- **Trilinear & Quadrilinear Velocity Interpolation** — High-performance 3D trilinear (spatial) and 4D quadrilinear (spatial + temporal) interpolation of wind fields directly inside GPU kernels in `puff_models.H` and `lpdm_models.H`
- **Spatially-Varying Canopy & Heterogeneous Surface Roughness** — Supports spatially distributed canopy height, frontal area index, and roughness ($z_0$) from user fields read into 2D AMReX arrays and retrieved cell-locally in solver kernels
- **Terrain-following** — Log-law wind profiles over complex topography
- **Synthetic Terrain Generation (EXPERIMENTAL)** — Programmatically generate single or multi-Gaussian hills directly using ParmParse configuration keys, without requiring an external terrain CSV file. This is highly useful for idealized simulations and testing wake/canopy parameters.
- **Multiple initialization modes** — Log-law, uniform, RAWS stations, HRRR-style surface parameters, power-law profiles, Deaves-Harris profiles, or log-law/power-law above boundary layer profiles
- **Pasquill-Gifford-Turner (PGT) Atmospheric Stability Diagnostics** — Decision-tree lookup matching ground wind speed, solar radiation (daytime), and cloud cover (nighttime) to A-F stability categories to compute dispersion coefficients when flux measurements are unavailable
- **Atmospheric Inversion Capping Lid (CALMET/CALPUFF-style)** — A defined mixing depth ($z_i$) acting as a physical boundary, enforcing $w = 0$ in the wind solver during Poisson solve, and reflecting dispersing pollutants downwards in the puff/particle dispersion solver. This supports both a flat, uniform input value and a cell-local, spatially-varying capping lid height $z_i(x,y)$ integrated directly from the 2D spatially-varying boundary layer depth MultiFab (`z_bl_diag_ptr`) or a file.
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
- **Dynamic Overwater Wave-Induced Roughness (Charnock's Relation)** — Computes aerodynamic roughness length dynamically over water grid cells using Charnock's relationship ($z_0 = \alpha_{ch} \frac{u_*^2}{g} + 0.11 \frac{\nu}{u_*}$) to account for wave height as a function of friction velocity.
- **Precipitation-Induced Atmospheric Stability Adjustments** — Automatically adjusts atmospheric stability indices (PGT classes or Monin-Obukhov lengths) toward neutral/stable regimes when local precipitation rates exceed a critical threshold (e.g., > 1 mm/h) from a time-varying precipitation input file.
- **Directional Bias Correction** — Corrects systematic wind direction and speed biases from NWP models
- **3D Meteorological Ingestion (NetCDF)** — Horizontal, terrain-aware vertical, and temporal interpolation of 3D NWP model outputs (e.g. WRF, GFS) into the solver grid. Includes anisotropic Inverse Distance Weighting (IDW) with vertical scaling parameter γ ≫ 1 to preserve vertical atmospheric profiles.
- **Topographic Barrier Shielding (CALMET-style)** — Zeroes out or heavily penalizes interpolation weights from stations across high terrain obstacles/ridges, preventing observations in one valley from unphysically influencing adjacent valleys
- **Solver enhancements** — Divergence damping filter, optional perturbation pressure gradient, multi-scale terrain analysis, smooth boundary layer transition
- **Performance timing** — Detailed timing output for profiling and optimization
- **Terrain-following coordinates** — Streamline coordinate transformation for improved accuracy on steep terrain
- **Gaussian puff dispersion** — Passive pollutant transport with enhanced physics:
  - **Adaptive Time-Stepping** — Dynamic scaling of Δt based on cell-local grid size and maximum wind velocity to ensure Courant–Friedrichs–Lewy (CFL) stability.
  - **Height-dependent diffusivity** — K(z) profiles for realistic atmospheric mixing
  - **First-order decay** — Exponential decay for radioactive/chemical species
  - **Plume rise** — Briggs buoyancy formula for heated sources
  - **Gravitational Settling & Dry Deposition** — Terminal settling velocity via Stokes' Law ($v_s = \frac{\rho_p d_p^2 g}{18\mu}$) and mass removal upon ground/vegetation contact
  - **Wet Deposition / Precipitation Scavenging** — Mass depletion of puffs or particles due to rain/snow washout via first-order scavenging ($\Lambda = \Lambda_0 \cdot P^a$)
  - **Ambient-Condition-Driven Chemical Decay** — Dynamic chemical half-lives affected by temperature (lapse rate), humidity, and photolysis/sunlight intensity
- **Enhanced diagnostics** — Surface heat flux and drag coefficient output fields, momentum flux, and spatially-varying boundary layer depth diagnosed via column-scanning bulk Richardson number ($Ri_b$) profile methods
- **Advanced physics features** — Simplified Richardson number stability classification, roughness blocking from buildings, latitude-dependent Coriolis parameter, power-law wind profile option above boundary layer, enhanced heat flux diagnostics, and building street canyon vortex parameterization
- **Python API** — Coupling with fire, atmospheric, and geochemical models including wind farm simulators (FLORIS, PyWake) and reactive transport solvers (PHREEQC)
  - **PHREEQC Reactive Transport Coupling** — One-way integration for critical mineral studies, acid mine drainage (AMD) analysis, and mineral weathering prediction. Exports terrain-resolved atmospheric boundary conditions (temperature, pressure, humidity, turbulent diffusivity, stability) as PHREEQC input to drive spatially-heterogeneous geochemical simulations. Enables identification of chemically-active "hotspots" driven by topographic wind steering and valley channeling. References: Parkhurst & Appelo (2013); Businger et al. (1971)
- **Turbine wake models** — Analytical wind turbine wake modeling (Jensen, Bastankhah Gaussian, TurbOPark, and Gauss-Curl Hybrid formulations) with quadratic/linear superposition, Jimenez and Bastankhah & Porté-Agel (2016) wake deflection under yawed conditions, height-varying (veered) wake coordinate projection to capture twisting, analytical wake-added turbulence (Crespo-Hernández and Frandsen models) with buoyant wake destruction under convective thermal conditions, and wake-ground interaction using mirroring and shear-damping techniques.
- **Environmental Dispersion and Multi-Scenario AEP Calculator** — Adds full-year wind farm assessment, pre-computed lookups, and environmental dispersion within wakes:
  - **Annual Energy Production (AEP) Calculator** — Python-based automated batch runner across a joint wind speed and direction distribution (wind rose), supporting sector-wise tracking and layout-level yaw sweep optimization.
  - **Fuga-style Linearized Wake Lookup** — Pre-computed 3D deficit look-up table (LUT) mapped onto the AMReX terrain mesh to bypass local analytical calculations in large wind farms.
  - **Integrated Turbine Wake-Induced Dispersion** — Couples analytical turbine wake-added turbulence (Crespo-Hernández, Frandsen) with both built-in Gaussian Puff and LPDM models to study pollutant or chemical transport and deposition within and around wind farms.
- **Lagrangian Particle Dispersion Model (LPDM) Enhancements**:
  - **Vertical Diffusivity Inhomogeneity Drift Correction** — Adds a vertical drift correction velocity $w_{drift} = \frac{\partial K_v}{\partial z}$ to the deterministic vertical advection of particles, eliminating numerical artifacts (spurious particle accumulation in regions of low vertical diffusivity) and ensuring physical uniformity in non-uniform vertical diffusivity $K_v(z)$ fields.
- **FLORIS & PyWake integration** — Export wind data to FLORIS wind farm simulation format, and format resolved wind fields as PyWake Site or WAsPGridSite objects
- **GPU-ready** — Runs on NVIDIA, AMD, and Intel GPUs via AMReX

## Synthetic Turbulence

The solver synthesizes **terrain-aware turbulent fluctuations** using:
- **Terrain-aware masking** — Fluctuations are confined to the fluid region and smoothly blended near terrain boundaries.
- **Spectral models** — Von Kármán or Kaimal spectrum options with height-dependent intensity.
- **BTS export** — OpenFAST/TurbSim compatible binary format for downstream simulations.

## Test Cases

Comprehensive test cases are located in the `test/` folder and documented in `test/README.md`.

## Regression Tests

Over 80 automated regression tests are located in `regtest/`, covering core mass-consistent wind solver components, advanced boundary layer dynamics, forest canopies, buildings and obstacles, environmental dispersion, synthetic turbulence, and analytical turbine wake models (including GCH, TurbOPark, Jimenez deflection, wake-added turbulence, and AEP calculator validation). It also includes a specialized test verifying Python-side one-way wildfire levelset coupling.

Run them using CTest from your build directory:
```bash
ctest -L regtest
```

## License

See [LICENSE](LICENSE).
