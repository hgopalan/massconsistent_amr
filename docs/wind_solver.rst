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

**5. Log-law initialisation** (GPU kernel)

For every cell (i, j, k):

.. math::

   z_\text{agl} = z_\text{lo} + (k+0.5)\,\Delta z - z_\text{terrain}(i,j)

Cells where z_agl ≤ 0 are inside the terrain (set to zero).  For cells above
the terrain:

.. math::

   u_0(i,j,k) = \frac{u_*}{\kappa}\ln\!\left(\frac{z_\text{agl}+z_0}{z_0}\right)\hat{u}_x, \quad
   v_0(i,j,k) = \frac{u_*}{\kappa}\ln\!\left(\frac{z_\text{agl}+z_0}{z_0}\right)\hat{u}_y, \quad
   w_0(i,j,k) = 0

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

The MLMG solver is configured with:

* Max iterations: 200
* Max FMG iterations: 20
* Pre/post smoothing steps: 16 each
* Bottom solver verbosity: 0

These defaults are conservative and suitable for most single-level problems.
For very large domains, reducing smoothing steps or increasing ``max_grid_size``
can improve throughput.

References
----------

* Sherman, C.A. (1978). A mass-consistent model for wind fields over complex
  terrain.  *Journal of Applied Meteorology*, 17(3), 312–319.
* Mathiesen, M. (1987). Simulation of wind fields in complex terrain.
  *Boundary-Layer Meteorology*, 38, 213–226.
* AMReX MLMG documentation:
  https://amrex-codes.github.io/amrex/docs_html/LinearSolvers.html
