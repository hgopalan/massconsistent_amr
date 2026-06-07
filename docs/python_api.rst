.. _python_api:

Python API
==========

The Mass-Consistent AMR Wind Solver provides robust, high-performance Python bindings through pybind11 and high-level object-oriented wrappers. This enables complete programmatic control, real-time data extraction, coupled wind-fire dispersion simulations, and synthetic turbulence studies directly from Python.

.. contents:: Topics
   :local:
   :depth: 2

Wind Solver Bindings (pyWindSolver)
-----------------------------------

The primary binding layer is compiled as a shared library and wrapped in an object-oriented Python module called ``wind_solver``.

Basic Usage
~~~~~~~~~~~
To initialize, solve, and extract velocities at specific altitudes:

.. code-block:: python

    from wind_solver import WindSolver

    # Initialize the solver from an input file
    wind = WindSolver("inputs.i")

    # Run the mass-consistent Poisson solve
    wind.solve()

    # Extract the 3D velocity field as a dictionary of NumPy arrays
    vel_3d = wind.get_velocity()
    u_field = vel_3d['u']  # NumPy array with shape (nz, ny, nx)

    # Extract 2D terrain-aligned velocity plane at 30 meters AGL
    vel_30m = wind.get_velocity_at_agl(30.0)
    u_30m = vel_30m['u']   # NumPy array with shape (ny, nx)

    # Clean up memory
    wind.finalize()

WindSolver Class Reference
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Constructor**:
* ``__init__(inputs_file)``: Initializes the C++ solver state and parses the ParmParse input deck.

**Methods**:
* ``solve()``: Triggers the mass-consistent Poisson adjustment.
* ``get_velocity()``: Returns a dictionary with keys ``'u', 'v', 'w'`` containing 3D velocity components.
* ``get_velocity_at_agl(height)``: Extracts the 2D horizontal plane of velocity at a given height above ground level (AGL). Returns a dictionary of 2D NumPy arrays.
* ``get_terrain()``: Returns a 2D NumPy array representing the interpolated terrain elevation.
* ``update_reference_wind(U_ref, V_ref)``: Re-evaluates friction velocities and updates reference flow parameters.
* ``write_plotfile(name)``: Writes the standard MultiFab cell-centered outputs in VisIt/ParaView compatible AMReX plotfile format.
* ``finalize()``: Destroys the C++ state singleton and cleans up AMReX runtime resources.

Coupled Wind-Fire Simulation Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Python bindings enable tight integration with external community fire spread solvers (such as ``wildfire_levelset``):

.. code-block:: python

    from wind_solver import WindSolver
    from wildfire_solver import WildfireSolver

    wind = WindSolver("wind_inputs.i")
    fire = WildfireSolver("fire_inputs.i")

    while fire.time < final_time:
        # 1. Update wind solver with latest atmospheric conditions
        wind.solve()
        vel_3d = wind.get_velocity()
        
        # 2. Feed wind field directly into the fire spread solver
        fire.update_wind_3d(vel_3d['u'], vel_3d['v'], vel_3d['w'],
                            wind.nz, wind.zmin, wind.zmax)
        
        # 3. Step fire front propagation
        fire.step()
        
    wind.finalize()
    fire.finalize()

Mann Box Spectral Turbulence Bindings
-------------------------------------

A specialized Python wrapper (``mann_box.py``) is provided for the Mann Box spectral tensor implementation, allowing for quick atmospheric stability studies and turbulence synthesis.

Basic Usage
~~~~~~~~~~~
To instantiate a Mann Box and compute spectral density components:

.. code-block:: python

    import numpy as np
    from mann_box import MannBox, create_mann_box_preset

    # Create a Mann Box instance utilizing a stable atmosphere preset
    mann = create_mann_box_preset('stable')

    # Compute spectral components at a set of frequencies
    freqs = np.logspace(-2, 1, 100)
    spectrum = mann.compute_spectrum(
        frequencies=freqs,
        height=90.0,
        mean_wind_speed=12.0
    )

    # Validate physical realizability (checks positive semi-definiteness)
    if mann.validate_realizability(spectrum):
        print("Synthesized spectra are physically realizable!")

MannBox Class Reference
~~~~~~~~~~~~~~~~~~~~~~~

**Constructor**:
* ``__init__(parameters=None)``: Initializes the spectral synthesis tensor. Takes an optional ``MannBoxParameters`` dataclass.

**Presets**:
* ``'neutral'``: Standard neutral atmospheric boundary layer.
* ``'stable'``: Nighttime stable layer with suppressed vertical fluctuations.
* ``'unstable'``: Daytime convective layer with enhanced mixing.
* ``'wind_farm'``: High shear, reduced length scales.
* ``'complex_terrain'``: High anisotropy, large length scales.

**Methods**:
* ``compute_spectrum(frequencies, height, mean_wind_speed)``: Computes the spectral tensor matrix. Returns a dictionary of spectral density components (``'S_uu', 'S_vv', 'S_ww', 'S_uw'``).
* ``validate_realizability(spectrum)``: Evaluates positive semi-definiteness and the Cauchy-Schwarz inequalities:
  
  .. math::
  
     |S_{uw}(f)|^2 \le S_{uu}(f) \cdot S_{ww}(f)

* ``update_parameters(**kwargs)``: Dynamically updates Monin-Obukhov parameters (length scales, intensities, coherence decay).

Build and Installation
----------------------

To build the Python bindings locally, configure CMake with the ``MASSCONSISTENT_BUILD_PYTHON_BINDINGS`` option turned on:

.. code-block:: bash

    cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
    cmake --build build --parallel

This compiles the ``pyWindSolver`` shared library into ``build/python/``. To make the module importable, append this folder to your python path:

.. code-block:: bash

    export PYTHONPATH=$PWD/build/python:$PYTHONPATH

To verify your installation, run the test suites:

.. code-block:: bash

    python3 src/python/test_wind_solver_api.py
    python3 test/test_gaussian_hill_mann_box.py
