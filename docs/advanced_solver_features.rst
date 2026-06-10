.. _advanced_solver_features:

Advanced Solver Features & Multi-Physics Integration
====================================================

**Full Spectral Tensor Synthesis, Advanced Physics, and Deep Integration**

This document outlines the advanced features including full spectral tensor synthesis, validation frameworks, and integration with multi-physics applications.

Overview
--------

The Advanced Solver Features layer extends the foundation provided by standard turbulence models (IEC 61400-1) and anisotropic spectral tensors (Mann Box) to deliver:

- **Complete 9-component spectral tensor synthesis** with full cross-component correlations
- **Eigenvalue decomposition** for physical realizability verification
- **GPU-accelerated FFT synthesis** for large-scale simulations
- **Non-neutral stratification coupling** with stability-dependent parameter adaptation
- **Advanced validation frameworks** with performance profiling and sensitivity analysis
- **Deep multi-physics integration** with fire, chemistry, and dispersion models

Architecture Overview
---------------------

The feature architecture is organized in layered capabilities:

**Foundation Layer**
- Basic wind profile initialization (log-law, power-law, RAWS stations)
- Simple spectral models (Von Kármán, IEC 61400-1)
- Basic turbulence intensity and coherence

**Intermediate Spectral Models**
- Mann Box anisotropic spectral tensor
- Terrain-dependent parameter adaptation
- Cross-component correlation modeling
- Python API bindings

**Validation & Optimization Layer**
- Performance profiling and throughput analysis
- Physical correctness validation
- Parameter sensitivity analysis
- GPU optimization suggestions

**Advanced Physics Layer**
- Non-neutral stability coupling
- Gravity wave effects
- Orographic precipitation interaction
- Coupled surface-atmosphere models

Full Spectral Tensor Synthesis
----------------------------------------

Current Implementation
~~~~~~~~~~~~~~~~~~~~~~

The Mann Box model currently implements:
- 1D spectral evaluation at individual wavenumber points
- Terrain-adaptive parameter modification
- Anisotropy representation through variance ratios
- Efficient computation for large domains

Extended Tensor Capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Full 3×3 spectral tensor synthesis enables:

**Complete Tensor Formulation**

.. math::

    S = \begin{pmatrix}
        S_{uu} & S_{uv} & S_{uw} \\
        S_{vu} & S_{vv} & S_{vw} \\
        S_{wu} & S_{wv} & S_{ww}
    \end{pmatrix}

where:
- Diagonal terms: Component spectra (streamwise, lateral, vertical)
- Off-diagonal terms: Cross-component spectra (correlations)

**Cross-Spectrum Representation**

Off-diagonal terms encode physical correlations:

- **S_uv**: Correlation between streamwise and lateral velocity fluctuations
  - Positive under wind shear
  - Captures wake meandering directionality
  
- **S_uw**: Correlation between streamwise and vertical velocity fluctuations
  - Typically negative (updrafts reduce forward motion)
  - Captures buoyancy-wind interaction
  
- **S_vw**: Correlation between lateral and vertical velocity fluctuations
  - Represents rotational coherent structures
  - Important for tilted vortex modeling

**Implementation Strategy**

1. Extend spectral tensor computation to evaluate all 9 components
2. Implement eigenvalue decomposition for realizability checks
3. Add cross-spectrum interpolation on 3D wavenumber grids
4. Enable GPU-accelerated complex FFT (cuFFT, rocFFT, oneAPI)

Time-Lag Correlation Structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Instantaneous Correlation
^^^^^^^^^^^^^^^^^^^^^^^^^^

Current implementation models spatial correlation only via coherence functions.

Enhanced Temporal Correlation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. math::

    \text{Cov}(u(\vec{r}), u(\vec{r}+\Delta\vec{r}, t+\Delta t)) = C(\vec{r}, \Delta t)

Benefits:
- Captures coherent burst structure
- Enables time-dependent turbulence passage modeling
- Improves wake meandering prediction
- More accurate pollutant dispersion

Implementation:
- Use Taylor frozen-flow hypothesis: Δt = Δx / U_mean
- Compute time-lagged spectral tensor
- Synthesize space-time correlated fluctuation fields

Eigenvalue Decomposition & Realizability Verification
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Current Implementation
^^^^^^^^^^^^^^^^^^^^^

The model currently enforces basic parameter bounds checking.

Enhanced Eigenvalue-Based Validation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The spectral tensor must satisfy:

.. math::

    \lambda_i \geq 0 \quad \forall i \in \{1, 2, 3\}

where λ_i are eigenvalues of S at each (k_x, k_y, k_z) point.

**Algorithm**:

1. Construct S from spectral components and parameters
2. Compute eigenvalue decomposition: S = Q Λ Q^T
3. Check non-negative eigenvalues: λ_i ≥ -ε_machine
4. If any λ_i < 0: Apply correction (damping, parameter adjustment)

**Correction Methods**:

- **Eigenvalue clipping**: λ_i = max(λ_i, 0)
- **Spectral damping**: Reduce anisotropy parameter
- **Parameter adjustment**: Modify variance ratios

GPU-Accelerated FFT Synthesis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

CPU-Based Spectral Evaluation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Current implementation evaluates spectral components sequentially on CPU.

Batch FFT on GPU Architecture
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Implementation Strategy**:

1. **GPU Memory Setup**:
   - Allocate device arrays for spectral tensor (9 × N_k grids)
   - Prepare random phase angles (GPU RNG)
   - Allocate output fluctuation fields

2. **GPU Spectral Computation**:
   - Evaluate all 9 spectral components on GPU
   - Apply eigenvalue decomposition (cuSolver/rocSolver)
   - Generate random Gaussian numbers

3. **GPU FFT Execution**:
   - Use cuFFT (NVIDIA), rocFFT (AMD), oneAPI (Intel)
   - Batch FFT for all 9 components simultaneously
   - Output space-domain fluctuations on GPU

4. **GPU Output Assembly**:
   - Combine mean wind + fluctuations
   - Apply terrain masking
   - Transfer to host if needed

**Performance Gains**:
- 50-100× speedup vs CPU for large domains
- Reduces CPU-GPU transfer overhead
- Enables real-time interactive simulations

Non-Neutral Stability Coupling
---------------------------------------

Current Implementation
~~~~~~~~~~~~~~~~~~~~~

- Log-law initialization with neutral Monin-Obukhov
- Optional Businger-Dyer stability correction
- Basic Richardson number diagnostics

Enhanced Stability Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Monin-Obukhov Length Feedback**

.. math::

    L_{MO} = \frac{u_*^2 T}{g \kappa (H_s / (\rho c_p))}

where:
- u_* = friction velocity
- H_s = sensible heat flux
- g = gravity
- κ = von Kármán constant

**Stability-Dependent Spectral Modification**

The spectral tensor shape changes with stability:

- **Unstable** (z/L_MO < -1): Shorter length scales, higher isotropy
- **Neutral** (|z/L_MO| < 0.1): Standard Mann Box or IEC model
- **Stable** (z/L_MO > 1): Longer length scales, reduced vertical motion

**Profile Modification Factors**

.. math::

    U(z) = \frac{u_*}{\kappa} \left[\ln\left(\frac{z}{z_0}\right) + \Phi_m(z/L_{MO})\right]

where Φ_m is the stability function:

- Businger-Dyer: Used in boundary layer meteorology
- Louis: Used in mesoscale models
- Högström: Recent refinements

**Temperature Coupling**

Buoyancy affects turbulence structure:

.. math::

    Ri = \frac{g}{T} \frac{dT}{dz} / \left(\frac{dU}{dz}\right)^2

- Ri < -0.05: Strongly unstable (enhanced turbulence)
- -0.05 < Ri < 0.25: Weakly stable/unstable
- Ri > 0.25: Strongly stable (suppressed turbulence)

Implementation Plan:
1. Add temperature field diagnostic
2. Compute Monin-Obukhov length from heat flux
3. Modify spectral parameters based on stability index
4. Update integral length scales and variance ratios dynamically

Advanced Physics: Gravity Wave Modeling
------------------------------------------

Gravity Wave Representation
~~~~~~~~~~~~~~~~~~~~~~~~~~~

In stable atmosphere over mountains, gravity waves become significant:

.. math::

    N = \sqrt{\frac{g}{\Theta} \frac{d\Theta}{dz}}

where N is the Brunt-Väisälä frequency.

**Implementation Strategy**:

1. Compute buoyancy frequency from temperature profile
2. Add gravity wave dispersion relation to spectral tensor
3. Generate vertically-coherent oscillations matching wave scales

Example Buoyancy-Wave Coupling:
- Large-scale oscillations above terrain (wavelength λ ~ 5-10 km)
- Vertical coherence across deep layers
- Phase tilt with height (leaning away from vertical wind)

Orographic Precipitation-Flow Interaction
---------------------------------------------------

Precipitation Feedback Mechanism
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Orographic precipitation modifies atmospheric stability:

1. **Latent Heat Release**: Cloud condensation warms air
2. **Stability Reduction**: Heating reduces Richardson number
3. **Flow Acceleration**: Less stable atmosphere allows stronger flow
4. **Feedback**: Faster flow → more evaporation → more instability

**Implementation Features**:

- Read precipitation fields from meteorological model or observations
- Compute heating rate from phase change: Q = L_v × condensation_rate
- Adjust temperature profile dynamically
- Update stability-dependent spectral parameters
- Re-solve wind field with modified stability

Coupled Surface-Atmosphere Modeling
---------------------------------------------

Integration with Fire Spread Models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The solver couples with fire simulation models:

1. **Wind Input**: Fire model receives mean wind from solver
2. **Heat Feedback**: Fire model outputs sensible heat flux
3. **Flow Modification**: Solver uses heat flux for next wind field
4. **Iterative Coupling**: Repeat until convergence

Example Workflow:
- t=0: Compute initial wind field
- t=1s-10s: Fire consumes fuel, releases heat
- t=10s: Wind solver uses heat flux to recompute wind field
- t=20s: Updated wind field affects fire spread
- t=30s: Cycle repeats

Implementation Approach:
- Pass heat flux field through C++ API
- Update surface boundary condition in wind solver
- Recompute wind field with new stability
- Return updated wind field to fire model

Integration with Chemistry Models (PHREEQC)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The solver couples with reactive transport models:

1. **Atmospheric Boundary Condition**: Solver outputs wind, temperature, humidity
2. **Mineral Dissolution**: Chemistry model computes oxidation rates
3. **Concentration Field**: Puff dispersion generates C(x,y,z)
4. **Leaching Efficiency**: Sherwood correlation links wind to dissolution
5. **Feedback**: Chemistry model may output outgassing, affecting heat

Current State (Phase 2):
- One-way coupling (wind → chemistry)
- No feedback from chemistry to wind

Phase 3+ Enhancement:
- Add feedback mechanisms for significant heat/momentum sources
- Couple with detailed geochemistry (PHREEQC reactive transport)
- Validation against field observations (AMD hotspots, sulfide oxidation)

Validation Framework (Phase 3)
------------------------------

Comprehensive validation is essential for advanced features.

Physical Bounds Validation
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Wind Field Constraints**:

- Speed: |u|, |v|, |w| < 50 m/s (prevent non-physical values)
- Divergence: ∇·u < 1e-10 (mass conservation)
- Reynolds number: Re = UL/ν > 0 (always positive)

**Spectral Constraints**:

- Positivity: S_ii ≥ 0 for all i (variance is always non-negative)
- Eigenvalues: λ_i ≥ 0 (tensor is positive semi-definite)
- Integral: ∫ S(k) dk = σ² (spectrum integrates to variance)

**Stability Constraints**:

- Temperature: T > 0 K (absolute zero)
- Richardson number: Ri_b ≥ 0 (always non-negative)
- Heat flux sign: Consistent with diurnal cycle

Performance Profiling
~~~~~~~~~~~~~~~~~~~~~

Track computational efficiency:

- **Wall-clock time**: Total elapsed time
- **CPU time**: Actual CPU usage (accounting for parallelization)
- **Memory usage**: Peak and average memory allocation
- **Convergence rate**: MLMG iterations and solver convergence
- **Throughput**: Grid points processed per second

Example Profiling Output:

.. code-block:: text

    ==================== PERFORMANCE REPORT ====================
    Wind Solver Timing (1000×1000×100 grid):
    
    Initialization:     12.3 ms
    Wind Profile:       45.2 ms
    Mass-Consistent:   234.5 ms
    Turbulence:        89.7 ms
    Output:            23.1 ms
    ────────────────────────────────
    Total:            404.8 ms
    
    Memory Usage:
    Grid (MultiFab):   156.2 MB
    Spectral Cache:     12.1 MB
    Working Space:      34.5 MB
    ────────────────────────────────
    Peak Total:        256.3 MB
    
    MLMG Solver Stats:
    Iterations:         18
    Bottom Solver:      Direct (LU)
    Convergence Rate:   0.12
    
    GPU Acceleration (CUDA):
    Kernels Launched:   45
    Device Memory:      512.0 MB
    Speedup vs CPU:     12.3×

Parameter Sensitivity Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Systematic study of parameter impact:

1. **Single Parameter Sweep**: Vary one parameter while holding others fixed
2. **Multi-Parameter Sweep**: Explore 2D/3D parameter space
3. **Interaction Analysis**: Identify parameter interdependencies
4. **Sensitivity Index**: Quantify relative importance

Example: Terrain Factor Sensitivity

.. code-block:: bash

    python3 tools/parameter_sensitivity.py \
        --inputs regtest/gaussian_hill/inputs.i \
        --param mann_terrain_adaptation_factor \
        --range 0.5 2.0 \
        --steps 15 \
        --output terrain_factor_sensitivity.csv

Output Analysis:
- Identify which parameters most strongly affect solution
- Document parameter uncertainties
- Guide future model development

Testing & Regression Suite
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Test Coverage**:

- Full tensor realizability: Eigenvalue computation
- FFT synthesis accuracy: Compare GPU vs CPU results
- Stability integration: Verify profile modifications
- Multi-physics coupling: Validate heat/momentum feedback loops

**CI/CD Integration**:

- Automated tests run on every commit
- GPU and CPU code paths tested in parallel
- Performance benchmarks track regressions
- Validation reports generated for each build

Feature Development Status
-------------------

**Foundation Layer (Stable, Production-Ready)**

- Von Kármán spectral synthesis (isotropic, IEC 61400-1 compliant)
- Basic wind profile initialization (log-law, power-law)
- Terrain-aware initialization with IDW interpolation
- Standard turbulence intensity classification

**Intermediate Spectral Models (Stable, Validated)**

- Mann Box anisotropic spectral tensor
- Terrain-dependent parameter adaptation
- Cross-component correlation support
- Python API bindings for interactive analysis

**Validation & Optimization Layer (In Development)**

- Performance profiling and throughput analysis
- Physical correctness validation framework
- Parameter sensitivity analysis suite
- GPU optimization suggestions

**Advanced Physics Layer (Future Development)**

- Non-neutral atmospheric stratification coupling via Monin-Obukhov length
- Gravity wave representation in stable atmosphere
- Orographic precipitation feedback mechanisms
- Coupled surface-atmosphere integration

Documentation & References
--------------------------

**Key Documentation Files**:

- ``docs/mann_model.rst`` — Anisotropic spectral tensor technical reference
- ``docs/iec61400_synthesis.rst`` — Standard-compliant turbulence synthesis
- ``docs/validation_optimization.rst`` — Validation framework and methodology
- ``docs/code_structure.rst`` — Implementation architecture and design
- ``docs/advanced_solver_features.rst`` — Advanced features and roadmap (this document)

**Test Suites**:

- ``tests_and_examples/mann_box/`` — Comprehensive spectral tensor tests
- ``regtest/turbulence/`` — Regression test suite for all turbulence models
- ``regtest/wakes/`` — Wake model integration and validation

**Python Modules**:

- ``src/python/mann_box.py`` — Mann Box Python bindings
- ``src/python/validation.py`` — Validation framework
- ``tools/parameter_sensitivity.py`` — Parameter sensitivity analysis tool

**References**:

1. Mann, J. (1994). The spatial structure of neutral atmospheric surface-layer turbulence. *Journal of Fluid Mechanics*, 273, 141-168.

2. Monin, A. S., & Obukhov, A. M. (1954). Basic laws of turbulent mixing in the ground layer of the atmosphere. *Trudy Geofiz. Inst. Akad. Nauk SSSR*, 24(151), 163-187.

3. Businger, J. A., Wyngaard, J. C., Izumi, Y., & Bradley, E. F. (1971). Flux-profile relationships in the atmospheric surface layer. *Journal of the Atmospheric Sciences*, 28(2), 181-189.

Getting Involved
----------------

**For Contributors**:

1. Review technical documentation (``docs/mann_model.rst``, ``docs/iec61400_synthesis.rst``)
2. Study test suites (``tests_and_examples/mann_box/``, ``regtest/``)
3. Check open issues on GitHub
4. Start with advanced physics features (full tensor, eigenvalue decomposition, stability coupling)

**For Users**:

1. Use foundation and intermediate layers in production (stable, well-validated)
2. Experiment with advanced physics features in development (subject to refinement)
3. Provide feedback on API usability and performance
4. Report bugs and suggest improvements

**Support**:

- GitHub Issues: Report bugs and request features
- Discussions: Ask questions and share experiences
- Pull Requests: Contribute code improvements
- Documentation: Help improve guides and examples

See Also
--------

- :ref:`mann_model` — Full Mann Box tensor documentation
- :ref:`iec61400_synthesis` — IEC turbulence models
- :ref:`validation_optimization` — Validation framework details
- :ref:`python_api` — Python API reference
- :ref:`code_structure` — Implementation architecture
