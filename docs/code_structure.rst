.. _code_structure:

Code Structure
==============

This section describes the repository architecture, file layout, class structures, and technical design of the Mass-Consistent AMR Wind Solver.

Directory Layout
----------------

The repository is organized into five main functional directories:

* **``src/``**: Core C++ mass-consistent solver implementation, physical parameterization libraries, and AMReX GPU kernels.
* **``src/python/``**: pyBind11 C++ bindings and the ``pyWindSolver`` wrapper providing NumPy-compatible zero-copy state extraction.
* **``tools/``**: Standalone Python scripts for terrain processing (SRTM), weather data, and FLORIS/VTK conversion.
* **``tests_and_examples/``**: Merged test cases, validation scripts, and example scripts for synthetic or alpine SRTM terrains and external couplings.
* **``regtest/``**: Automated regression suite verifying stability corrections, wakes, and puff dispersion.

Core C++ Source File Reference
------------------------------

The C++ codebase is designed to be highly modular, using header-only physical parameterizations coupled with AMReX multi-level multigrid pipelines.

Core Solver Pipeline
~~~~~~~~~~~~~~~~~~~~

* **``src/wind_solver.cpp``**: Main executable entry point. Handles command-line arguments, ParmParse configuration, terrain point cloud ingestion, grid generation, the MLMG Poisson solve, and plotfile export.
* **``src/wind_solver_api.h`` / ``src/wind_solver_api.cpp``**: Complete C-style integration API. Exposes solver internals to the Python runtime, allowing external steering.

Atmospheric and Obstacle Physics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **``src/canopy_models.H``**: Portably evaluates MacDonald displacement height, effective canopy roughness lengths, and Shaw-Pereira exponential decay velocity fields directly within device kernels.
* **``src/wake_models.H``**: Implements Röckle, Huber-Snyder, and AERMOD PRIME wake deficit models. Computes projected building areas (PBA) under arbitrary building rotation angles, applies distance-weighted adaptive wake blending, and evaluates analytical turbine wake profiles (Jensen & Bastankhah).
* **``src/puff_models.H``**: High-performance device kernels for passive Gaussian pollutant dispersion. Handles 3D advection, growth, first-order decay, Briggs plume rise, gravitational settling, dry/wet deposition, and AERMOD PRIME cavity trapping.
* **``src/puff_solver.cpp``**: Main standalone executable compiling ``puff_models.H`` with a uniform-wind time-stepping solver.

Synthetic Turbulence & Fluctuation Pipeline
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **``src/synthetic_turbulence.H``**: Computes component RMS targets, integral length scales, and coherence decay from Monin-Obukhov parameters.
* **``src/random_field_synthesis.H``**: Performs reproducible three-dimensional fluctuation synthesis from Von Kármán or Kaimal amplitude spectra.
* **``src/temporal_synthesis.H``**: Synthesizes temporally-correlated fluctuation sequences using integral timescales.
* **``src/turbsim_bts_export.H``**: Serializes synthesized spatial-temporal sequences into OpenFAST/TurbSim binary BTS format.

AMReX MultiFab Data Layout
--------------------------

To achieve parallel scalability and portable GPU execution, the solver relies on AMReX MultiFabs (multi-component, distributed arrays). The typical layout of the primary cell-centered MultiFab includes:

====== ======================== ========== ===========
Index  Name                     Units      Description
====== ======================== ========== ===========
0-2    u, v, w                  m/s        Velocity components (corrected)
3      vel_magnitude            m/s        Wind speed
4-6    u0, v0, w0               m/s        Initial wind velocity components
7      lambda                   m²/s       Lagrange multiplier
8      div_before               1/s        Divergence before correction
9      div_after                1/s        Divergence after correction
10     terrain_z                m          Terrain elevation
11     heat_flux                W/m²       Surface sensible heat flux
12     drag_coeff               —          Surface drag coefficient
13-15  tau_x, tau_y, u_star     Pa, m/s    Momentum flux components
16-17  richardson_no, bl_depth  —, m       Boundary layer diagnostics
18-19  div_damped_before/after  1/s        Divergence damping
20-21  pressure_pert, res       Pa, 1/s    Pressure Poisson perturbation
22-24  terrain_type/slope/curv  —, —, 1/m  Terrain analysis classification
25-27  transition_weight, etc.  —, m/s     Surface-to-mixed layer smoothing
====== ======================== ========== ===========

GPU Execution Model
-------------------

The solver uses the **AMReX GPU execution model**, which compiles physical kernels into CUDA (NVIDIA), HIP (AMD), or SYCL (Intel) device code based on the selected CMake build options.

All grid-sweeping loops are structured using ``amrex::ParallelFor``:

.. code-block:: cpp

   amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
   {
       // Device kernel executes in parallel on all GPU threads
       Real z_agl = ComputeHeightAGL(i, j, k, terrain_z);
       if (z_agl > 0.0) {
           u0(i,j,k) = ComputeLogLawProfile(z_agl, u_star, z0);
       } else {
           u0(i,j,k) = 0.0; // Solid terrain
       }
   });

This ensures that:
1. There is no host-device memory transfer within the time-stepping or Poisson solve loops.
2. The code scales seamlessly from a single CPU thread to thousands of high-performance GPU nodes.
