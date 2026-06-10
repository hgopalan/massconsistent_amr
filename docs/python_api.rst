.. _python_api:

Python API
==========

The solver provides Python bindings through pybind11 with high-level object-oriented wrappers for programmatic control, data extraction, wind-fire coupling, and synthetic turbulence generation.

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

PyWake Coupling and Site Export
-------------------------------

The ``pywake_coupling`` module provides classes and utilities to format converged mass-consistent wind fields directly into PyWake ``Site`` or ``WAsPGridSite`` structures.

Basic Usage
~~~~~~~~~~~
To export to a custom PyWake ``Site`` subclass or save as standard WAsP GRD files:

.. code-block:: python

    from wind_solver import WindSolver
    from pywake_coupling import MassConsistentSite, to_wasp_grid_site

    wind = WindSolver("inputs.i")
    wind.solve()

    # 1. Create a PyWake-compatible Site object for local wind queries
    site = MassConsistentSite(wind)
    local_wind = site.local_wind(x=[100, 200], y=[150, 150], h=[90.0, 90.0])

    # 2. Export WAsP Surfer .grd ASCII grid files
    wasp_site = to_wasp_grid_site(wind, height_agl=90.0, output_dir="wasp_grids")

    wind.finalize()

AEP Production Calculation (AEPCalculator)
------------------------------------------

The ``aep_calculator`` module automates batch execution of the mass-consistent C++ wind solver across a joint wind speed and direction distribution (wind rose) to compute total Annual Energy Production (AEP).

Basic Usage
~~~~~~~~~~~
To calculate AEP across a 4-direction, 3-speed wind rose:

.. code-block:: python

    import numpy as np
    from aep_calculator import AEPCalculator

    # 1. Instantiate AEPCalculator with inputs file
    calc = AEPCalculator("inputs.i")

    # 2. Define wind rose parameters
    wind_speeds = [5.0, 10.0, 15.0]
    wind_directions = [0.0, 90.0, 180.0, 270.0]
    
    # Joint probabilities summing to 1.0
    probabilities = [
        [0.1,  0.1,  0.05],  # North (0 deg)
        [0.15, 0.15, 0.05],  # East  (90 deg)
        [0.05, 0.05, 0.05],  # South (180 deg)
        [0.1,  0.1,  0.1 ]   # West  (270 deg)
    ]

    # 3. Execute batch simulations
    res = calc.run_wind_rose(wind_speeds, wind_directions, probabilities)

    # 4. Extract total AEP in kWh and detailed profiles
    results = res["results"]
    print(f"Total Annual Energy Production: {results['total_aep_kwh']:.2f} kWh")
    print(f"Sector-by-sector AEP (kWh):", results["sector_aep_kwh"])
    print(f"Turbine-by-turbine AEP (kWh):", results["turbine_aep_kwh"])

AEPCalculator Class Reference
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Constructor**:
* ``__init__(inputs_file)``: Initializes the calculator and stores the path to the C++ solver input template.

**Methods**:
* ``run_wind_rose(wind_speeds, wind_directions, probabilities, turbines=None, yaw_offsets=None, stability_scenarios=None)``: Runs the batch simulations. Optionally accepts custom turbine listings, specific yaw offset angles, and stability parameter scenarios to perform rapid layout optimization under varying conditions.

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
    python3 tests_and_examples/mann_box/test_gaussian_hill_mann_box.py
