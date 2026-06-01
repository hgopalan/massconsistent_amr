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

**NEW**: The solver supports spatially-varying aerodynamic roughness length
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

