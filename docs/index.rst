.. Mass-Consistent AMR Wind Solver documentation master file

Mass-Consistent AMR Wind Solver Documentation
===============================================

Welcome to the documentation for the Mass-Consistent AMR Wind Solver — a
terrain-following, mass-consistent 3-D wind diagnostic tool built on
`AMReX <https://amrex-codes.github.io/amrex/>`_.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   overview
   mathematical_models
   code_structure
   building
   usage
   python_api
   tools
   infrastructure
   scenarios
   regtests
   validation_optimization
   iec61400_synthesis
   mann_model
   advanced_solver_features
   external_coupling
   references

Overview
========

``massconsistent_amr`` implements the QUIC-URB style Lagrange multiplier
mass-consistent wind adjustment method (Sherman 1978).  Given a terrain
elevation file and a reference wind speed, the solver:

* Constructs a **log-law initial wind field** over complex terrain, evaluating
  the von Kármán profile at the height above local terrain (AGL) for every grid
  cell.
* Enforces **mass consistency** (∇·\ **u** = 0) by solving an anisotropic
  Poisson equation via the AMReX MLMG linear solver (``MLABecLaplacian``).
* Writes the corrected, divergence-free wind field as an **AMReX plotfile** and
  optionally a terrain-aligned CSV slice.

Key Features
------------

* **Terrain-aware initialization** — IDW interpolation of arbitrary-density
  terrain point clouds onto the computational grid.
* **Log-law profile** — von Kármán constant, aerodynamic roughness length,
  and reference height all configurable.
* **Mass-consistent correction** — anisotropic Poisson equation with independent
  horizontal (α_h) and vertical (α_v) penalty coefficients.
* **AMReX MLMG solver** — robust multi-level multigrid; configurable tolerances
  and verbosity.
* **GPU-ready** — AMReX GPU kernels (CUDA, HIP, SYCL) for all field operations.
* **Terrain-aligned CSV output** — sample the corrected field at any AGL height.
* **Building wake modeling** — Röckle (1990) parameterization with rooftop vortices
  and arbitrary building orientations.
* **Canopy effects** — Forest canopy drag modeling for vegetated terrain.
* **Puff dispersion** — Gaussian puff transport for pollutant modeling.
* **Python API** — Coupling interface for fire and atmospheric models.

Quick Start
-----------

Clone with submodules::

    git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
    cd massconsistent_amr

Build and run::

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ./build/wind_solver regtest/gaussian_hill/inputs.i

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
