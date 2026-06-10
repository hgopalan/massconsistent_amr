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
equations. [Sherman1978]_

The approach in ``massconsistent_amr`` follows the variational formulation of
Sherman (1978) and Mathiesen (1987), adapted for the AMReX framework to enable
modern CPU/GPU portability and scalable parallel execution. [Mathiesen1987]_
This diagnostic methodology is also implemented in regulatory and research
models such as QUIC-URB and is foundational to many operational wind field
modeling systems. [PardyjaksREF]_

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

The log-law wind profile follows the Monin-Obukhov similarity theory:
[BusingerEtAl1971]_

.. math::

   u(z_\text{agl}) = \frac{u_*}{\kappa}\,\ln\!\left(\frac{z_\text{agl}+z_0}{z_0}\right)

with the friction velocity

.. math::

   u_* = \frac{\kappa\,|\mathbf{U}_\text{ref}|}
              {\ln\!\left(\dfrac{z_\text{ref}+z_0}{z_0}\right)}

where κ = 0.41 is the von Kármán constant, z₀ is the aerodynamic roughness
length, and z_ref is the reference height above the local terrain surface.
For non-neutral stability conditions, corrections following Businger-Dyer
or Holtslag-De Bruin formulations are applied. [DyerREF]_ [HoltslagDebruinREF]_

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
  shape over rolling terrain. [PardyjaksREF]_

Advanced Physics Models
-----------------------

The solver includes optional advanced physics parameterizations:

* **Atmospheric stability** — Monin-Obukhov similarity theory with Businger-Dyer
  stability functions for non-neutral boundary layers [StullREF]_
* **Thermal buoyancy** — Boussinesq approximation for temperature-driven vertical motion
* **Kinematic terrain BC** — No-flow-through boundary condition at terrain surface
* **Ekman spiral** — Wind direction veer with height due to Coriolis effects
* **Elevation scaling** — Wind speed variation with terrain elevation
* **Building porosity** — Porous flow through structures (trees, fences)
* **Wall functions** — Log-law boundary conditions for coarse-grid simulations
* **Canopy drag** — Forest canopy parameterization (MacDonald et al. 2000) [MacdonaldShawPereira2000]_
* **Building wakes** — Röckle (1990) wake model for urban flows [RockleREF]_
* **Jackson-Hunt orographic acceleration** — Wind acceleration over convex terrain [JacksonHunt1975]_

See :ref:`detailed physics documentation <mathematical_models>` for more information.

Comparison with Related Tools
------------------------------

``massconsistent_amr`` is positioned alongside diagnostic wind and dispersion solvers like QUIC-URB/QUIC-Plume and WindNinja, and regulatory dispersion systems like AERMOD/AEROMOD and CALPUFF. The following table summarizes key features and capabilities:

.. list-table::
   :widths: 20 20 20 20 20
   :header-rows: 1

   * - Feature
     - massconsistent_amr
     - QUIC (QUIC-URB / QUIC-Plume)
     - WindNinja
     - AERMOD / AEROMOD + CALPUFF
   * - **Wind Solver**
     - ✓ (Lagrange mass-consistent)
     - ✓ (Lagrange mass-consistent)
     - ✓ (Lagrange mass-consistent / CFD)
     - ✗ (None; reads pre-computed wind fields)
   * - **GPU Acceleration**
     - ✓ (CUDA/HIP/SYCL via AMReX)
     - ✗ (CPU only)
     - Limited (OpenMP CPU multi-threading)
     - ✗ (CPU only)
   * - **Turbulence Models**
     - ✓ (Mann Box spectral, Von Kármán, Kaimal)
     - ✓ (Röckle wake, local diagnostic)
     - Limited (Basic Monin-Obukhov)
     - ✓ (Boundary layer similarity, PDF models)
   * - **Obstacles & Wake**
     - ✓ (Röckle, Huber-Snyder, AERMOD PRIME)
     - ✓ (Röckle wake parameterization)
     - ✗ (No building-level support)
     - ✓ (PRIME downwash algorithm in AERMOD)
   * - **Pollutant Dispersion**
     - ✓ (Integrated Gaussian puff, Briggs plume rise, K(z))
     - ✓ (Lagrangian dispersion via QUIC-Plume)
     - ✗ (Wind solver only)
     - ✓ (Steady-state plume [AERMOD] & Lagrangian puff [CALPUFF])
   * - **Parallel Scalability**
     - ✓ (High-performance MPI + GPU via AMReX)
     - Limited (OpenMP / basic multi-threading)
     - Limited (OpenMP multi-threading)
     - Limited (Embarrassingly parallel batching)
   * - **Open Source**
     - ✓ (Open source on GitHub)
     - ✗ (Proprietary / Restricted access)
     - ✓ (Open source on GitHub)
     - ✓ (US EPA regulatory open-source)

.. note::

   This comparison highlights differences across distinct modeling philosophies, from diagnostic microscale flow solvers to regulatory meso/macroscale transport models. For design certification, consult official US EPA or international regulatory guidelines. Wildfire-specific features and GUI elements are excluded. For current information, consult the respective project documentation.

   QUIC-URB references: [PardyjaksREF]_, AERMOD references: [CimorelliREF]_, CALPUFF references: [ScireREF]_.

AMReX Integration
-----------------

``massconsistent_amr`` uses the following AMReX components:

* ``MultiFab`` — distributed multi-component field storage
* ``Geometry`` / ``BoxArray`` / ``DistributionMapping`` — parallel domain
  decomposition
* ``MLABecLaplacian`` / ``MLMG`` — multi-level multigrid Poisson solver
* ``ParallelFor`` / ``AMREX_GPU_DEVICE`` — portable CPU/GPU kernels
* ``WriteSingleLevelPlotfile`` — AMReX plotfile output (VisIt / ParaView compatible)

The AMReX framework provides high-performance multigrid solvers suitable for
mass-consistent applications on structured adaptive meshes. [AMReXDocs]_

References
----------

Please see the complete list of scientific publications and frameworks on the :ref:`references page <references>`.

.. [Sherman1978] Sherman, C. A. (1978). A mass-consistent model for wind fields over complex terrain. *Journal of Applied Meteorology*, 17(3), 312–319.
.. [Mathiesen1987] Mathiesen, M. (1987). Simulation of wind fields in complex terrain. *Boundary-Layer Meteorology*, 38, 213–226.
.. [PardyjaksREF] Pardyjak, E. R., & Brown, M. J. (2001). *QUIC-URB v. 1.1: Theory and User's Guide*. Los Alamos National Laboratory, LA-UR-01-4228.
.. [BusingerEtAl1971] Businger, J. A., Wyngaard, J. C., Izumi, Y., & Bradley, E. F. (1971). Flux-profile relationships in the atmospheric surface layer. *Journal of Atmospheric Sciences*, 28(2), 181–189.
.. [DyerREF] Dyer, A. J. (1974). A review of flux-profile relationships. *Boundary-Layer Meteorology*, 7(3), 363–372.
.. [HoltslagDebruinREF] Holtslag, A. A. M., & De Bruin, H. A. R. (1988). Applied modeling of the nighttime surface energy balance over land. *Journal of Applied Meteorology*, 27, 689–704.
.. [StullREF] Stull, R. B. (1988). *An Introduction to Boundary Layer Meteorology*. Kluwer Academic Publishers.
.. [MacdonaldShawPereira2000] MacDonald, R. W., Griffiths, R. F., & Hall, D. J. (1998). An improved method for the estimation of surface roughness of obstacle arrays. *Journal of Applied Meteorology*, 37(12), 1857–1864.
.. [RockleREF] Röckle, R. (1990). *Bestimmung der Strömungsverhältnisse im Bereich komplexer Bebauungsstrukturen*. PhD thesis, TH Darmstadt.
.. [JacksonHunt1975] Jackson, P. S., & Hunt, J. C. R. (1975). Turbulent wind flow over a low hill. *Quarterly Journal of the Royal Meteorological Society*, 101, 929–955.
.. [CimorelliREF] Cimorelli, A. J., et al. (2005). AERMOD: A dispersion model for air quality regulatory modeling. *Journal of the Air & Waste Management Association*, 55(9), 1322–1331.
.. [ScireREF] Scire, J. S., Strimaitis, D. G., & Yamartino, R. J. (1992). A User's Guide for the CALPUFF Dispersion Model (Version 5). Earth Tech, Inc.
.. [AMReXDocs] AMReX Collaboration (2023). AMReX: A framework for building massively parallel block-structured adaptive mesh refinement applications. https://github.com/AMReX-Codes/amrex
