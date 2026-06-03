.. _overview:

Overview
========

``massconsistent_amr`` is a terrain-following, mass-consistent 3-D wind
diagnostic tool built on the `AMReX <https://amrex-codes.github.io/amrex/>`_
adaptive-mesh framework.  It is designed for high-resolution wind modelling
over complex terrain and serves as a building block for atmospheric and
wildfire-spread simulations.

Background
----------

Accurate wind fields over complex terrain are critical for many environmental
applications — wildfire behaviour prediction, pollutant dispersion, wind energy
resource assessment, and urban airflow modelling.  Diagnostic mass-consistent
models offer a practical balance between physical fidelity and computational
cost: they adjust a simple initial wind profile (e.g., log-law) to satisfy
mass conservation (∇·\ **u** = 0) without solving the full Navier–Stokes
equations.

The approach in ``massconsistent_amr`` follows the variational formulation of
Sherman (1978) and Mathiesen (1987), adapted for the AMReX framework to enable
modern CPU/GPU portability and scalable parallel execution.

Physical Model
--------------

**Step 1 — Terrain interpolation**

An arbitrary-density terrain point cloud (X, Y, Z) is read from a CSV file.
The terrain elevation at each grid column centre is obtained by inverse-distance
weighting (IDW) interpolation using the six nearest data points.

**Step 2 — Log-law initialisation**

For every grid cell (i, j, k) the height above ground level (AGL) is:

.. math::

   z_\text{agl}(i,j,k) = z_\text{physical}(k) - z_\text{terrain}(i,j)

where z_physical(k) = z_lo + (k + 0.5) × dz and z_terrain(i,j) is the IDW
terrain elevation.  Cells where z_agl ≤ 0 are inside the terrain and are
set to zero.

The log-law wind profile is:

.. math::

   u(z_\text{agl}) = \frac{u_*}{\kappa}\,\ln\!\left(\frac{z_\text{agl}+z_0}{z_0}\right)

with the friction velocity

.. math::

   u_* = \frac{\kappa\,|\mathbf{U}_\text{ref}|}
              {\ln\!\left(\dfrac{z_\text{ref}+z_0}{z_0}\right)}

where κ = 0.41 is the von Kármán constant, z₀ is the aerodynamic roughness
length, and z_ref is the reference height above the local terrain surface.

**Step 3 — Mass-consistent correction**

The corrected wind field is found by minimising

.. math::

   E = \int\!\left[\frac{(u-u_0)^2}{\alpha_h^2}
                  +\frac{(v-v_0)^2}{\alpha_h^2}
                  +\frac{(w-w_0)^2}{\alpha_v^2}\right]\mathrm{d}V

subject to ∇·\ **u** = 0 (mass conservation).  Applying Lagrange multipliers
gives the anisotropic Poisson equation for λ:

.. math::

   -\!\left(\alpha_h^2\frac{\partial^2\lambda}{\partial x^2}
           +\alpha_h^2\frac{\partial^2\lambda}{\partial y^2}
           +\alpha_v^2\frac{\partial^2\lambda}{\partial z^2}\right)
   = -\nabla\cdot\mathbf{u}_0

with boundary conditions:

* x-faces (inflow/outflow): Dirichlet λ = 0
* y-faces (lateral): Neumann ∂λ/∂y = 0
* z-faces (ground, top): Neumann ∂λ/∂z = 0

**Step 4 — Wind correction**

The final divergence-free wind field is:

.. math::

   \mathbf{u} = \mathbf{u}_0
    - \left(\alpha_h^2\frac{\partial\lambda}{\partial x},\;
            \alpha_h^2\frac{\partial\lambda}{\partial y},\;
            \alpha_v^2\frac{\partial\lambda}{\partial z}\right)

Anisotropy Coefficients
-----------------------

The parameters α_h and α_v control the relative weight given to horizontal
versus vertical adjustments:

* **α_h = α_v = 1** (default): isotropic correction.  Horizontal and vertical
  velocities are adjusted equally.
* **α_v < α_h** (e.g. α_h = 1, α_v = 0.01): vertical velocity is penalised
  more heavily, so the solver preferentially adjusts horizontal winds.  This
  is similar to the QUIC-URB default and tends to preserve the log-law profile
  shape over rolling terrain.

Advanced Physics Models
-----------------------

The solver includes optional advanced physics parameterizations:

* **Atmospheric stability** — Monin-Obukhov similarity theory with Businger-Dyer
  stability functions for non-neutral boundary layers
* **Thermal buoyancy** — Boussinesq approximation for temperature-driven vertical motion
* **Kinematic terrain BC** — No-flow-through boundary condition at terrain surface
* **Ekman spiral** — Wind direction veer with height due to Coriolis effects
* **Elevation scaling** — Wind speed variation with terrain elevation
* **Building porosity** — Porous flow through structures (trees, fences)
* **Wall functions** — Log-law boundary conditions for coarse-grid simulations
* **Canopy drag** — Forest canopy parameterization (MacDonald et al. 2000)
* **Building wakes** — Röckle (1990) wake model for urban flows

See :ref:`physics` for detailed physics documentation.

Comparison with Related Tools
------------------------------

``massconsistent_amr`` is positioned alongside other mass-consistent wind solvers like
QUIC-URB (Pardyjak & Brown 2001) and WindNinja. The following table summarizes key
feature comparisons (excluding wildfire/visualization features specific to those codes):

.. list-table::
   :widths: 25 25 25 25
   :header-rows: 1

   * - Feature
     - massconsistent_amr
     - QUIC-URB
     - WindNinja
   * - **Mass-consistent solver**
     - ✓ (Lagrange multiplier)
     - ✓ (Lagrange multiplier)
     - ✓ (Lagrange multiplier)
   * - **GPU acceleration**
     - ✓ (CUDA/HIP/SYCL via AMReX)
     - ✗ (CPU only)
     - Limited (OpenMP)
   * - **Terrain-following grid**
     - ✓ (terrain-following coords)
     - ✓ (terrain-following coords)
     - ✓ (terrain-following coords)
   * - **Building wake models**
     - ✓ (Röckle, Huber-Snyder, AERMOD PRIME)
     - ✓ (Röckle parameterization)
     - ✓ (Basic wake effects)
   * - **Canopy drag**
     - ✓ (MacDonald et al. 2000, exponential)
     - ✓ (MacDonald et al. 2000)
     - ✓ (Vegetation drag)
   * - **Non-neutral stability**
     - ✓ (Businger-Dyer, Holtslag-De Bruin)
     - ✓ (Businger-Dyer)
     - ✓ (Monin-Obukhov)
   * - **Thermal buoyancy**
     - ✓ (Boussinesq approximation)
     - Limited
     - ✓ (Temperature-driven)
   * - **Position-dependent roughness**
     - ✓ (file or NLCD classification)
     - ✓ (file based)
     - ✓ (landuse classification)
   * - **Directional bias correction**
     - ✓ (Core feature)
     - ✗
     - Limited
   * - **Surface flux diagnostics**
     - ✓ (SHF, LHF, τ, C_d)
     - Limited diagnostics
     - Basic flux output
   * - **Gap flow parameterization**
     - ✓ (pressure-driven channeling)
     - ✗
     - Limited
   * - **Valley channeling**
     - ✓ (geometry-based model)
     - Basic
     - Basic
   * - **Froude number terrain blocking**
     - ✓ (stable stratification)
     - Basic
     - Basic
   * - **Multiple initialization modes**
     - ✓ (log-law, uniform, RAWS, surface_data)
     - Limited
     - Limited (NWP data)
   * - **Python API**
     - ✓ (Model coupling interface)
     - ✗
     - Limited
   * - **MPI parallelism**
     - ✓ (via AMReX)
     - Limited
     - Limited
   * - **Open source**
     - ✓ (GitHub)
     - ✗ (Proprietary)
     - ✓ (OpenWind model)

.. note::

   This comparison focuses on the core mass-consistent wind solver capabilities.
   Wildfire-specific features and GUI elements are excluded. For current information,
   consult the respective project documentation.

AMReX Integration
-----------------

``massconsistent_amr`` uses the following AMReX components:

* ``MultiFab`` — distributed multi-component field storage
* ``Geometry`` / ``BoxArray`` / ``DistributionMapping`` — parallel domain
  decomposition
* ``MLABecLaplacian`` / ``MLMG`` — multi-level multigrid Poisson solver
* ``ParallelFor`` / ``AMREX_GPU_DEVICE`` — portable CPU/GPU kernels
* ``WriteSingleLevelPlotfile`` — AMReX plotfile output (VisIt / ParaView compatible)

References
----------

* Sherman, C.A. (1978). A mass-consistent model for wind fields over complex
  terrain.  *Journal of Applied Meteorology*, 17(3), 312–319.
* Mathiesen, M. (1987). Simulation of wind fields in complex terrain.
  *Boundary-Layer Meteorology*, 38, 213–226.
* Röckle, R. (1990). *Bestimmung der Strömungsverhältnisse im Bereich
  komplexer Bebauungsstrukturen*.  PhD thesis, TH Darmstadt.
* Pardyjak, E.R. & Brown, M.J. (2001). *QUIC-URB v. 1.1: Theory and User's Guide*.
  Los Alamos National Laboratory, LA-UR-01-4228.
* AMReX: https://github.com/AMReX-Codes/amrex
