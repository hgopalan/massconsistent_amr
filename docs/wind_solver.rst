.. _wind_solver:

Wind Solver Implementation
==========================

This page describes the implementation details of the mass-consistent 3-D
wind solver in ``src/wind_solver.cpp``.

Source File: ``src/wind_solver.cpp``
-------------------------------------

The entire solver is implemented as a single self-contained C++ source file.
It uses AMReX for grid management, data distribution, GPU-portable field
operations, and the MLMG linear solver.

Execution Flow
--------------

The ``main()`` function performs the following steps in order:

**1. Parse inputs** (``ParmParse``)

All solver parameters are read from an ``inputs.i`` file or from command-line
``key=value`` pairs via AMReX ``ParmParse``.

**2. Read terrain**

The terrain point cloud (X, Y, Z) is read from a whitespace- or
comma-separated CSV file.  Lines beginning with ``#`` are treated as comments.
The horizontal domain extents (x_lo, x_hi, y_lo, y_hi) are derived from the
bounding box of the terrain data.

**3. Build grid**

Grid dimensions ``(nx, ny, nz)`` are computed from the domain extents and the
requested cell spacings ``(dx, dy, dz)``.  The vertical domain spans
[z_lo, z_hi] where z_lo = min terrain elevation and z_hi = max terrain
elevation + ``domain_height``.

An AMReX ``Geometry``, ``BoxArray``, and ``DistributionMapping`` are
constructed for parallel execution.

**4. IDW terrain interpolation**

For each horizontal column (i, j) the terrain elevation at the column centre
is estimated by inverse-distance-weighting (IDW) using the six nearest terrain
data points with inverse-square-distance weights.  The result is stored in a
host vector and copied to the device for use in GPU kernels.

**5. Wind field initialisation** (GPU kernel)

The wind initialization method depends on the ``init_mode`` parameter:

* **loglaw** (default): Log-law profile with global U_ref, V_ref, z_ref, z0
* **uniform**: Constant wind field (uniform_U, uniform_V)
* **raws**: Interpolate from velocity file (X Y Z U V format)
* **surface_data**: HRRR-style surface parameters (X Y Z USTAR Z0 U10 V10)

For **loglaw** mode, every cell (i, j, k):

.. math::

   z_\text{agl} = z_\text{lo} + (k+0.5)\,\Delta z - z_\text{terrain}(i,j)

Cells where z_agl ≤ 0 are inside the terrain (set to zero).  For cells above
the terrain:

.. math::

   u_0(i,j,k) = \frac{u_*}{\kappa}\ln\!\left(\frac{z_\text{agl}+z_0}{z_0}\right)\hat{u}_x, \quad
   v_0(i,j,k) = \frac{u_*}{\kappa}\ln\!\left(\frac{z_\text{agl}+z_0}{z_0}\right)\hat{u}_y, \quad
   w_0(i,j,k) = 0

For **surface_data** mode, the solver reads surface parameters (friction velocity,
roughness length, 10m winds) from a data file, interpolates them to each column
using IDW, then constructs per-column vertical profiles using the local parameters.
This enables spatially-varying surface characteristics suitable for HRRR data
ingestion.

**6. Compute divergence** (GPU kernel)

The RHS of the Poisson equation is computed as:

.. math::

   \text{rhs}(i,j,k) = -\nabla\cdot\mathbf{u}_0

By default, centred differences are used in the interior and one-sided differences
at domain boundaries.  Alternatively, the ``deriv_method`` parameter allows using
WENO-3 or WENO-5 schemes for improved accuracy near discontinuities or steep gradients.
Sub-surface cells are skipped (rhs = 0).

**7. MLMG Poisson solve**

An ``MLABecLaplacian`` operator is set up with:

* α_a = 0 (no identity term)
* β_b = 1 (full diffusion)
* B coefficients: bx = by = α_h², bz = α_v²
* Boundary conditions: Dirichlet on x-faces, Neumann on y- and z-faces

``MLMG`` solves for the Lagrange multiplier λ to the requested relative
tolerance (default 1e-8).

**8. Correct velocity** (GPU kernel)

.. math::

   u = u_0 - \alpha_h^2\,\partial\lambda/\partial x, \quad
   v = v_0 - \alpha_h^2\,\partial\lambda/\partial y, \quad
   w = w_0 - \alpha_v^2\,\partial\lambda/\partial z

One-sided or one-sided/upwind gradients are used at domain boundaries (depending
on the ``deriv_method``).  Sub-surface cells are reset to zero.

**9. Compute divergence diagnostics**

The divergence before and after correction is computed for diagnostic
purposes.  The solver prints the maximum absolute divergence (as ``max|div|``)
before and after correction; the post-correction value should be at or below
the MLMG tolerance multiplied by the maximum divergence before correction.

**10. Write output**

The corrected wind field, initial wind field, Lagrange multiplier, divergence
diagnostics, and terrain elevation are written to an AMReX plotfile via
``WriteSingleLevelPlotfile``.

If ``extract_agl`` or ``extract_k`` is set, a terrain-aligned 2-D CSV slice
is also written.

Unified Field Output (Phase 5)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The solver consolidates all diagnostic fields into a unified 21-component output structure
via the ``FieldOutput.H`` module. The output MultiFab contains:

**Wind Components (4 fields):**
- ``u``, ``v``, ``w`` — Corrected wind components [m/s]
- ``vel_magnitude`` — Horizontal wind speed |U| [m/s]

**Initial Wind Field (3 fields):**
- ``u0``, ``v0``, ``w0`` — Log-law initial field [m/s]

**Mass Consistency (3 fields):**
- ``lambda`` — Lagrange multiplier (normalized pressure variable)
- ``div_before`` — Divergence of initial field ∇·u₀ [1/s]
- ``div_after`` — Divergence of corrected field ∇·u [1/s]

**Terrain & Geometry (1 field):**
- ``terrain_z`` — Terrain elevation [m MSL]

**Surface Flux Diagnostics (5 fields):**
- ``heat_flux`` — Sensible heat flux SHF = ρ Cp u* θ* [W/m²]
- ``drag_coeff`` — Drag coefficient Cd = (κ/ln(z/z₀))² [dimensionless]
- ``tau_x``, ``tau_y`` — Shear stress components [Pa]
- ``u_star`` — Friction velocity [m/s]

**Boundary Layer & Stability (2 fields):**
- ``richardson_no`` — Richardson number Ri_b (bulk) [dimensionless]
- ``bl_depth`` — Boundary layer depth [m]

**Terrain Analysis (3 fields):**
- ``terrain_type`` — Classification of terrain (0=smooth, 1=rough, etc.)
- ``terrain_slope`` — Local terrain slope magnitude [dimensionless]
- ``adaptive_z0`` — Adaptive aerodynamic roughness [m]

The module provides standardized field naming, automatic field enumeration, and
helper functions for computing friction velocity, drag coefficient, and momentum flux
diagnostics on GPU-portable kernels.

**Usage in C++:**

.. code-block:: cpp

    #include "FieldOutput.H"
    
    // Compute friction velocity
    Real ustar = FieldOutput::ComputeFrictionVelocity(u_mag, z_agl, z0);
    
    // Get standardized field name
    std::string name = FieldOutput::GetFieldName(FieldOutput::FieldIndex::HEAT_FLUX);
    
    // Get all field names
    auto var_names = FieldOutput::GetStandardVarNames();
    WriteSingleLevelPlotfile(plot_file, output, var_names, geom, 0.0, 0);

Key Data Structures
-------------------

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Variable
     - Type
     - Description
   * - ``vel0``
     - ``MultiFab(ba, dm, 3, 1)``
     - Initial log-law wind field (3 components, 1 ghost cell)
   * - ``vel_c``
     - ``MultiFab(ba, dm, 3, 0)``
     - Mass-corrected wind field (no ghost cells needed for output)
   * - ``lam``
     - ``MultiFab(ba, dm, 1, 1)``
     - Lagrange multiplier λ (1 ghost cell for gradient stencil)
   * - ``rhs``
     - ``MultiFab(ba, dm, 1, 0)``
     - Poisson RHS = −∇·\ **u**₀
   * - ``d_terr``
     - ``Gpu::DeviceVector<Real>``
     - Per-column terrain elevation on device (size nx × ny)

GPU Portability
---------------

All field-level loops use ``amrex::ParallelFor`` with
``AMREX_GPU_DEVICE`` lambdas, making the code portable across CUDA, HIP,
and SYCL backends.  The terrain elevation vector is copied to the device
once before the kernel launches and accessed via a raw device pointer.

The source file must be compiled as CUDA (``LANGUAGE CUDA``) when the CUDA
backend is selected.  This is handled automatically by CMake when
``MASSCONSISTENT_GPU_BACKEND=CUDA``.

Solver Settings
---------------

The MLMG solver can be tuned via the following input parameters:

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - Parameter
     - Default
     - Description
   * - ``mlmg_max_iter``
     - 200
     - Maximum MLMG iterations
   * - ``mlmg_max_fmg_iter``
     - 20
     - Maximum Full Multigrid (FMG) iterations
   * - ``mlmg_pre_smooth``
     - 16
     - Pre-smoothing iterations per V-cycle
   * - ``mlmg_post_smooth``
     - 16
     - Post-smoothing iterations per V-cycle
   * - ``mlmg_bottom_solver``
     - default
     - Bottom solver: ``default``, ``bicgstab``, ``cg``, or ``smoother``
   * - ``tol_rel``
     - 1.0e-8
     - Relative convergence tolerance
   * - ``max_grid_size``
     - 32
     - Maximum AMReX box size per dimension

**Performance Tuning Guidelines:**

* **For well-conditioned problems:** Reduce ``mlmg_pre_smooth`` and ``mlmg_post_smooth`` 
  to 8–12 for faster convergence
* **For highly anisotropic problems** (α_h/α_v > 100): Use ``bicgstab`` or ``cg`` 
  bottom solver for better convergence
* **For GPU acceleration:** Increase ``max_grid_size`` to 64–256 for cache-friendly 
  operations and better GPU occupancy
* **For very large domains:** Reduce smoothing iterations and consider using 
  ``bicgstab`` bottom solver

**Example input for aggressive tuning:**

.. code-block:: text

   mlmg_max_iter = 100
   mlmg_max_fmg_iter = 10
   mlmg_pre_smooth = 8
   mlmg_post_smooth = 16
   max_grid_size = 128  # GPU optimization

Position-Dependent Roughness
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The solver supports spatially-varying aerodynamic roughness length
z₀(x,y) for more realistic land-use heterogeneity.

**Usage:**

.. code-block:: text

   # Enable position-dependent roughness
   z0_file = roughness.csv

where ``roughness.csv`` contains:

.. code-block:: text

   # X [m]  Y [m]  Z0 [m]
   0.0      0.0    0.03    # Grass
   500.0    0.0    0.5     # Forest
   1000.0   0.0    0.0001  # Water

The roughness is interpolated to each grid column using inverse-distance
weighting (IDW) with the 6 nearest points, enabling smooth transitions
between land-use types.

**Typical roughness values:**

.. list-table::
   :header-rows: 1
   :widths: 40 20

   * - Land Use Type
    - z₀ [m]
   * - Open water
    - 0.0001 - 0.001
   * - Grass/crops
    - 0.01 - 0.05
   * - Shrubs/bushes
    - 0.1 - 0.3
   * - Forest
    - 0.5 - 1.0
   * - Urban/suburban
    - 0.5 - 2.0

**Performance Timing:**

The solver now reports detailed timing for each major phase:

* Input parsing
* Terrain reading and interpolation
* Grid setup
* Wind field initialization
* RHS computation
* Poisson operator setup
* Poisson solve (core solver time)
* Velocity correction
* Divergence diagnostics
* Output writing

Use these timings to identify bottlenecks in your workflow.

References
----------

* Sherman, C.A. (1978). A mass-consistent model for wind fields over complex
  terrain.  *Journal of Applied Meteorology*, 17(3), 312–319.
* Mathiesen, M. (1987). Simulation of wind fields in complex terrain.
  *Boundary-Layer Meteorology*, 38, 213–226.
* AMReX MLMG documentation:
  https://amrex-codes.github.io/amrex/docs_html/LinearSolvers.html

Canopy Parameterization
-----------------------

The solver includes vegetation canopy models that modify the wind profile
within and above canopies. Two models are implemented:

**1. MacDonald et al. (2000) - Displacement Height Model**

This empirical model computes an effective displacement height ``d`` and
roughness length ``z0_eff`` based on canopy morphology parameters:

* Frontal area index λ_f = frontal area / ground area
* Plan area index λ_p = plan area / ground area  
* Drag coefficient C_d (typically 0.2-0.3)

The displacement height is computed as:

.. math::

   d/H = 1 + \alpha^{-\lambda_p}(\lambda_p - 1), \quad \alpha = 4.43

The effective roughness length is:

.. math::

   z_0/H = (1 - d/H) \exp\left(-\sqrt{\frac{1}{0.5 \beta C_d (1-d/H) \lambda_f}}\right), \quad \beta = 1.0

The modified log-law profile becomes:

.. math::

   u(z) = \frac{u_*}{\kappa}\ln\!\left(\frac{z - d + z_0}{z_0}\right)

for z > d, and u(z) = 0 for z ≤ d.

**2. Shaw & Pereira (1982) - Exponential Decay Model**

When ``use_exponential_profile = true``, the wind speed within the canopy
(z < h) follows an exponential decay:

.. math::

   u(z) = u(h) \exp\left(-\alpha\left(1 - \frac{z}{h}\right)\right)

where α is the attenuation coefficient (typically 2-4). Above the canopy
(z ≥ h), the standard log-law with displacement height applies.

**Usage Example**

To enable canopy effects in an input file::

    enable_canopy = true
    canopy_height = 15.0          # Forest canopy height [m]
    frontal_area_index = 0.25     # Moderately dense forest
    plan_area_index = 0.20
    canopy_drag_coeff = 0.2
    
    # Optional: use exponential decay within canopy
    use_exponential_profile = true
    canopy_attenuation = 2.5

**Typical Parameter Values**

.. list-table::
   :header-rows: 1
   :widths: 30 35 35

   * - Canopy Type
     - λ_f (frontal)
     - λ_p (plan)
   * - Sparse forest
     - 0.15 - 0.20
     - 0.10 - 0.15
   * - Moderate forest
     - 0.25 - 0.30
     - 0.20 - 0.25
   * - Dense forest
     - 0.35 - 0.45
     - 0.30 - 0.40
   * - Crops/grassland
     - 0.10 - 0.15
     - 0.05 - 0.10

**References**

* MacDonald, R.W., Griffiths, R.F., Hall, D.J. (2000). A comparison of
  results from scaled field and wind tunnel modelling of dispersion in arrays
  of obstacles. *Atmospheric Environment*, 34(20), 3845-3862.
* Shaw, R.H., Pereira, A.R. (1982). Aerodynamic roughness of a plant canopy:
  A numerical experiment. *Agricultural Meteorology*, 26, 51-65.
* Cionco, R.M. (1965). A mathematical model for air flow in a vegetative
  canopy. *Journal of Applied Meteorology*, 4, 517-522.

Ekman Spiral Wind Veer Correction
----------------------------------

The solver includes an Ekman spiral correction that adds wind direction
rotation (veer) with height due to Coriolis effects. This is important for
large-scale atmospheric flows and wind energy applications.

**Physical Background**

In the atmospheric boundary layer, winds are influenced by:

1. Pressure gradient force (drives geostrophic wind aloft)
2. Surface friction (retards near-surface wind)
3. Coriolis force (due to Earth's rotation)

The balance between these forces causes the wind direction to rotate with
height — a phenomenon known as the Ekman spiral. In the Northern Hemisphere,
winds typically veer (rotate clockwise) with height; in the Southern Hemisphere,
they back (rotate counter-clockwise).

**Implementation**

The wind veer is applied using an exponential profile:

.. math::

   \\theta(z) = \\theta_{\\text{total}} \\times [1 - \\exp(-z / h_{\\text{veer}})]

where:

* :math:`\\theta(z)` is the veer angle at height z [radians]
* :math:`\\theta_{\\text{total}}` is the total veer from surface to domain top
* :math:`h_{\\text{veer}}` is the height scale for the veer profile

The horizontal wind components are then rotated:

.. math::

   \\begin{aligned}
   u(z) &= u_{\\text{base}} \\cos(\\theta) - v_{\\text{base}} \\sin(\\theta) \\\\
   v(z) &= u_{\\text{base}} \\sin(\\theta) + v_{\\text{base}} \\cos(\\theta)
   \\end{aligned}

where :math:`u_{\\text{base}}` and :math:`v_{\\text{base}}` are computed from
the selected wind profile (log-law, power-law, or surface_data).

**Usage Example**

To enable Ekman veer in an input file::

    enable_ekman_veer = true
    latitude = 45.0              # Mid-latitude Northern Hemisphere
    ekman_veer_total = 25.0      # 25 degrees total veer
    ekman_veer_height = 150.0    # Most veer in lowest 150 m

**Typical Parameter Values**

.. list-table::
   :header-rows: 1
   :widths: 30 35 35

   * - Region
     - Total Veer [degrees]
     - Notes
   * - Low latitudes (0-30°)
     - 5 - 15
     - Weak Coriolis effect
   * - Mid latitudes (30-60°)
     - 15 - 30
     - Typical for most applications
   * - High latitudes (60-90°)
     - 30 - 45
     - Strong Coriolis effect
   * - Stable conditions
     - +50% enhancement
     - Increased stratification
   * - Unstable conditions
     - -30% reduction
     - Enhanced mixing

**Height Scale Guidelines**

The veer height :math:`h_{\\text{veer}}` controls how quickly wind direction
changes with height:

* **100-150 m**: Shallow boundary layer or stable conditions
* **150-200 m**: Typical neutral conditions
* **200-300 m**: Deep convective boundary layer

**Compatibility**

Ekman veer works with all wind initialization modes:

* **loglaw**: Veer is applied after computing log-law speed
* **powerlaw**: Veer is applied after computing power-law speed
* **surface_data**: Veer is applied to each column's profile

The mass-consistency solver adjusts the final wind field to enforce
:math:`\\nabla \\cdot \\mathbf{u} = 0`, while preserving the veer structure
as much as possible.

**Validation and Testing**

See regression test ``regtest/ekman_veer/`` for a complete example demonstrating
wind veer on flat terrain. The test extracts wind at multiple heights to verify
the veer profile.

**References**

* Ekman, V.W. (1905). On the influence of the Earth's rotation on ocean currents.
  *Arkiv för Matematik, Astronomi och Fysik*, 2(11).
* Arya, S.P. (1988). *Introduction to Micrometeorology*. Academic Press, Ch. 9.
* Stull, R.B. (1988). *An Introduction to Boundary Layer Meteorology*. Kluwer, Ch. 9.
* IEC 61400-1 Ed. 4 (2019). Wind energy generation systems — Part 1: Design requirements.
  Section 6.3.1.3: Wind veer for turbine load analysis.

Orographic Speed-up and Flow Separation
----------------------------------------

The solver includes an orographic speed-up model based on the Jackson & Hunt (1975)
theory for flow over low hills and ridges. This feature accounts for wind acceleration
over convex terrain features (ridges, hill crests) and flow separation/deceleration
in concave regions (valleys, lee slopes).

**Physical Background**

When wind flows over terrain, it experiences:

1. **Speed-up on windward slopes and crests**: Wind accelerates as it flows up and
   over ridges and hills due to streamline compression
2. **Flow separation on lee slopes**: Wind decelerates and can separate from steep
   lee slopes, creating recirculation zones
3. **Vertical decay**: Speed-up effects are strongest near the surface and decay
   with height above the terrain

The Jackson & Hunt (1975) linear theory provides a framework for predicting these
effects for hills with shallow slopes (H/L << 1) where H is hill height and L is
the characteristic horizontal length scale.

**Implementation**

The orographic speed-up is computed based on local terrain slope and curvature:

.. math::

   \\Delta S(z) = S_{\\text{max}} \\times s \\times C \\times \\exp(-z / L_z)

where:

* :math:`\\Delta S` is the fractional speed change (speedup or slowdown)
* :math:`S_{\\text{max}}` is the maximum speedup factor (typically 1.5-2.0)
* :math:`s` is the local terrain slope magnitude
* :math:`C` is a curvature strength indicator (0-1)
* :math:`z` is height above ground level
* :math:`L_z` is the vertical decay length scale (typically L/2)

**Curvature-based classification:**

* **Positive curvature** (convex features like ridges, hill tops): Apply speedup
  
  .. math::
  
     u_{\\text{new}} = u_{\\text{base}} \\times (1 + \\Delta S)

* **Negative curvature** (concave features like valleys, lee slopes): Apply slowdown
  
  .. math::
  
     u_{\\text{new}} = u_{\\text{base}} \\times (1 - \\Delta S_{\\text{sep}})

  where :math:`\\Delta S_{\\text{sep}}` accounts for flow separation strength

**Usage Example**

To enable orographic speed-up in an input file::

    enable_orographic_speedup = true
    orographic_hill_length_scale = 100.0      # Characteristic hill half-length [m]
    orographic_speedup_factor_max = 2.0       # Maximum speedup on ridges
    orographic_separation_factor = 0.3        # Flow separation strength
    orographic_smoothing_factor = 0.5         # Terrain smoothing (0-1)

**Parameter Guidelines**

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Parameter
     - Typical Value
     - Notes
   * - ``hill_length_scale``
     - 50-200 m
     - Half-width of dominant terrain features
   * - ``speedup_factor_max``
     - 1.5-2.5
     - Maximum wind acceleration on ridges (2.0 is typical)
   * - ``separation_factor``
     - 0.2-0.5
     - Strength of lee-side deceleration (0.3 is typical)
   * - ``smoothing_factor``
     - 0.3-0.7
     - Controls transition smoothness (higher = smoother)

**Terrain Analysis**

The model computes local terrain characteristics using finite differences:

* **Slope magnitude**: :math:`|\\nabla h| = \\sqrt{(\\partial h/\\partial x)^2 + (\\partial h/\\partial y)^2}`
* **Curvature**: :math:`\\nabla^2 h = \\partial^2 h/\\partial x^2 + \\partial^2 h/\\partial y^2`

These are computed from neighboring terrain elevations using central differences,
providing second-order accuracy on uniform grids.

**Compatibility**

Orographic speed-up works with all wind initialization modes:

* **loglaw**: Applied after log-law profile computation
* **powerlaw**: Applied after power-law profile computation
* **uniform**: Applied to uniform wind field
* **raws/surface_data**: Applied after interpolation

The speedup is applied to the initial wind field before the mass-consistency
correction, so the final field enforces :math:`\\nabla \\cdot \\mathbf{u} = 0`
while retaining terrain-induced speed variations.

**Validation and Testing**

See regression test ``regtest/orographic_speedup/`` for a complete example
demonstrating speedup over a Gaussian hill.

**Limitations**

* Assumes shallow slopes (H/L < 0.3 for linearity)
* Neutral atmospheric stability (extension to stable/unstable conditions requires
  combining with ``enable_stability_correction``)
* Does not model fine-scale turbulence or three-dimensional vortex shedding
* Vertical component (w) is adjusted by mass-consistency solver, not directly
  by the speedup model

**References**

* Jackson, P.S., & Hunt, J.C.R. (1975). Turbulent wind flow over a low hill.
  *Quarterly Journal of the Royal Meteorological Society*, 101(430), 929-955.
* Winstral, A., Marks, D., & Gurney, R. (2013). Simulating wind fields and snow
  redistribution using terrain-based parameters to model snow accumulation and melt
  over a semi-arid mountain catchment. *Hydrological Processes*, 27(26), 3973-3998.
* Forthofer, J.M., Butler, B.W., & Wagenbrenner, N.S. (2014). A comparison of three
  approaches for simulating fine-scale surface winds in support of wildland fire
  management. Part I. Model formulation and comparison against measurements.
  *International Journal of Wildland Fire*, 23(7), 969-981.

Gap Flow Parameterization
-------------------------

The solver includes a gap flow parameterization model for mountain passes and
valleys where pressure-driven channeling creates enhanced wind speeds. Gap flows
are important for wind energy assessment, aviation, and regional wind patterns,
with typical speed-up factors of 2-4× synoptic wind speeds.

**Physical Background**

Mountain gaps and passes create natural wind corridors:

1. **Pressure gradient**: Temperature and pressure differences across the gap
   drive flow through the constriction
2. **Channeling**: Flow is forced through the narrow gap, accelerating due to
   mass continuity
3. **Speed-up**: Gap winds can reach 2-4× synoptic wind speeds (e.g., Columbia
   River Gorge can exceed 40 m/s)
4. **Directional alignment**: Flow aligns with gap axis, potentially reversing
   synoptic flow direction

Classic examples include the Columbia River Gorge (Washington/Oregon), Strait of
Gibraltar, and numerous Alpine passes.

**Implementation**

The gap flow parameterization consists of three components:

1. **Gap geometry detection** — identifies points within or near the gap based on
   distance from gap center and alignment with gap axis

2. **Pressure difference calculation** — computes pressure gradient across the gap
   from synoptic wind speed and orientation:

   .. math::

      \\Delta p = C \\cdot \\frac{1}{2}\\rho U_{syn}^2 \\cdot \\cos(\\theta_{align}) \\cdot \\left(1 + \\frac{H}{W}\\right)

   where:

   * :math:`C` is the pressure coefficient
   * :math:`\\rho` = 1.225 kg/m³ (air density)
   * :math:`U_{syn}` is synoptic wind speed
   * :math:`\\theta_{align}` is angle between wind and gap axis
   * :math:`H` is gap depth (elevation range)
   * :math:`W` is gap width

3. **Gap flow velocity** — enhanced wind speed from Bernoulli equation:

   .. math::

      U_{gap} = \\sqrt{\\frac{2\\Delta p}{\\rho}}

   The gap flow is aligned with the gap axis and blended with base wind based on
   distance from gap center and height above ground.

**Vertical and Horizontal Structure**

* **Vertical decay**: Gap flow influence decreases exponentially with height
  above ground:

  .. math::

     f(z) = \\exp(-z / H_{gap})

  where :math:`H_{gap}` is the vertical extent parameter (typically 500-1500 m)

* **Horizontal transition**: Smooth transition from full gap flow (inside gap) to
  ambient flow (outside gap) using cosine taper over transition width

**Usage Example**

To enable gap flow parameterization in an input file::

    enable_gap_flow = true
    gap_flow_orientation = 90.0        # Gap axis orientation [degrees, 0=east, 90=north]
    gap_flow_width = 1000.0            # Gap width [m]
    gap_flow_depth = 500.0             # Gap depth (elevation range) [m]
    gap_flow_pressure_coefficient = 1.0  # Pressure-driven flow coefficient
    gap_flow_speedup_max = 3.0         # Maximum gap flow speedup (typically 2-4)
    gap_flow_center_x = 5000.0         # Gap center X coordinate [m]
    gap_flow_center_y = 5000.0         # Gap center Y coordinate [m]
    gap_flow_transition_width = 500.0  # Transition zone width [m]
    gap_flow_vertical_extent = 1000.0  # Vertical extent of gap flow influence [m]

**Parameter Guidelines**

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Parameter
     - Typical Value
     - Notes
   * - ``gap_flow_orientation``
     - 0-360°
     - Direction of gap axis (0=east, 90=north, 180=west, 270=south)
   * - ``gap_flow_width``
     - 500-5000 m
     - Width of gap (smaller = stronger channeling)
   * - ``gap_flow_depth``
     - 200-1000 m
     - Vertical elevation range of gap
   * - ``gap_flow_pressure_coefficient``
     - 0.5-1.5
     - Tuning parameter for pressure-driven flow strength
   * - ``gap_flow_speedup_max``
     - 2.0-4.0
     - Maximum wind speed enhancement (Columbia Gorge: 3-4×)
   * - ``gap_flow_transition_width``
     - 200-1000 m
     - Width of transition zone at gap edges

**Applications**

* **Wind energy**: Gap winds create concentrated high-speed zones ideal for wind
  farms
* **Aviation**: Strong gap flows can create hazardous conditions for aircraft
* **Fire behavior**: Rapid fire spread in gap wind corridors
* **Regional climate**: Gap winds affect temperature and precipitation patterns

**Compatibility**

Gap flow works with all wind initialization modes:

* **loglaw**: Applied after log-law profile computation
* **powerlaw**: Applied after power-law profile computation
* **uniform**: Applied to uniform wind field
* **raws/surface_data**: Applied after interpolation

The gap flow is applied to the initial wind field before the mass-consistency
correction, so the final field enforces :math:`\\nabla \\cdot \\mathbf{u} = 0`
while retaining gap-induced speed variations.

**Validation and Testing**

See regression test ``regtest/gap_flow_mountain/`` for a complete example
demonstrating gap flow through a mountain pass.

**Limitations**

* Assumes steady-state gap flow (transient ramp-up not modeled)
* Simplified gap geometry (rectangular cross-section)
* Does not resolve turbulence or vortex shedding at gap edges
* Pressure gradient simplified from full 3-D pressure field

**References**

* Mass, C.F., & Albright, M.D. (1987). Coastal southerlies and alongshore surges
  of the west coast of North America: Evidence of mesoscale topographically
  trapped response to synoptic forcing. *Monthly Weather Review*, 115(8),
  1707-1738.
* Sharp, J., & Mass, C.F. (2004). Columbia Gorge gap winds: Their climatological
  influence and synoptic evolution. *Weather and Forecasting*, 19(6), 970-992.
* Jackson, P.L., Mayr, G., & Vosper, S. (2013). Dynamically-driven winds.
  *Mountain Weather Research and Forecasting*, Springer, 121-218.


