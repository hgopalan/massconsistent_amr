.. _python_api:

Python API
==========

Summary
-------

This implementation adds Python bindings to the massconsistent_amr wind solver to support **complete solver control from Python**, enabling coupled wind-fire simulations with external fire solvers like wildfire_levelset.

What Was Implemented
--------------------

1. Wind Solver State Management (``wind_solver_api.H`` / ``.cpp``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**New C++ API functions:**

* ``wind_solver_initialize(inputs_file)`` — Initialize from inputs file
* ``wind_solver_solve()`` — Solve for mass-consistent wind field
* ``wind_solver_get_velocity()`` — Extract 3D velocity field
* ``wind_solver_get_velocity_at_agl(height)`` — Extract velocity at specific AGL
* ``wind_solver_get_terrain()`` — Extract terrain elevation
* ``wind_solver_update_reference_wind()`` — Update wind and re-initialize
* ``wind_solver_write_plotfile()`` — Write AMReX plotfile
* ``wind_solver_finalize()`` — Clean up

**Key Features:**

* Global state singleton persists between Python calls
* Stores all MultiFabs, geometry, terrain, and solver parameters
* Handles AMReX initialization automatically
* Supports multiple solve cycles with parameter updates

2. Enhanced Python Bindings (``pyWindSolver.cpp``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Extended pybind11 module with:**

* ``pyWindSolver.initialize()`` — Initialize wind solver
* ``pyWindSolver.solve()`` — Solve for mass-consistent wind
* ``pyWindSolver.get_velocity()`` — Extract 3D velocity as numpy arrays
* ``pyWindSolver.get_velocity_at_agl()`` — Extract velocity at AGL height
* ``pyWindSolver.get_terrain()`` — Extract terrain elevation
* ``pyWindSolver.update_reference_wind()`` — Update wind parameters
* ``pyWindSolver.write_plotfile()`` — Write AMReX plotfile
* ``pyWindSolver.finalize()`` — Cleanup

**Data Conversion:**

* Automatic conversion between C++ MultiFabs and numpy arrays
* Fortran order (column-major) for compatibility with numpy
* Proper shape handling: (nz, ny, nx) for 3D fields, (ny, nx) for 2D fields

3. High-Level Python Wrapper (``wind_solver.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Object-oriented API:**

.. code-block:: python

    wind = WindSolver("inputs.i")
    wind.solve()
    velocity = wind.get_velocity()
    wind.write_plotfile("plt_wind")
    wind.finalize()

**Features:**

* Clean, Pythonic interface
* Context manager support (``with WindSolver(...) as wind:``)
* Automatic error checking and validation
* Comprehensive docstrings
* Property accessors for solver state

4. Coupled Simulation Example (``coupled_wind_fire_example.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Demonstrates:**

* Initialization of both wind and fire solvers
* Solving mass-consistent wind field
* Passing 3D wind data to fire solver
* Time loop with periodic wind updates
* Plotfile writing for both solvers
* Statistics reporting

5. Comprehensive Test Suite (``test_wind_solver_api.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Three test cases:**

1. Basic initialization from inputs file
2. Solve and extract wind fields
3. High-level WindSolver class usage

**Validates:**

* Initialization success
* MLMG solver convergence
* Velocity extraction correctness
* Terrain data accuracy
* API error handling

6. Updated Build System (``CMakeLists.txt``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**CMake changes:**

* Added ``MASSCONSISTENT_BUILD_PYTHON_BINDINGS`` option
* Python3 and pybind11 detection/fetching
* pyWindSolver module compilation
* wind_solver_api library target
* Position-independent code for Python bindings
* Python file installation

Usage Examples
--------------

Simple Wind Solve
~~~~~~~~~~~~~~~~~

.. code-block:: python

    from wind_solver import WindSolver

    wind = WindSolver("inputs.i")
    wind.solve()

    # Extract velocity at 10m AGL
    vel_agl = wind.get_velocity_at_agl(10.0)
    print(f"Mean wind at 10m: U={vel_agl['u'].mean():.2f} m/s")

    wind.finalize()

Coupled Wind-Fire
~~~~~~~~~~~~~~~~~

.. code-block:: python

    from wind_solver import WindSolver
    from wildfire_solver import WildfireSolver  # from wildfire_levelset

    # Initialize solvers
    wind = WindSolver("wind_inputs.i")
    fire = WildfireSolver("fire_inputs.i")

    # Solve wind
    wind.solve()

    # Get 3D wind and pass to fire
    vel = wind.get_velocity()
    fire.update_wind_3d(vel['u'], vel['v'], vel['w'], 
                        wind.nz, wind.zmin, wind.zmax)

    # Run fire simulation
    for n in range(num_steps):
        fire.step()
        state = fire.get_state()
        print(f"t={state['time']:.1f}s")

    # Cleanup
    wind.finalize()
    fire.finalize()

Build Instructions
------------------

Configure with Python bindings::

    cmake -S . -B build \
      -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON

Build::

    cmake --build build -j

Set PYTHONPATH::

    export PYTHONPATH=$PWD/build/python:$PYTHONPATH

Run tests::

    python3 src/python/test_wind_solver_api.py

Run coupled example (requires wildfire_levelset)::

    python3 src/python/coupled_wind_fire_example.py

Integration with wildfire_levelset
-----------------------------------

Once both solvers have Python bindings (as implemented in PR #230 for wildfire_levelset), the coupled workflow becomes:

.. code-block:: python

    from wind_solver import WindSolver
    from wildfire_solver import WildfireSolver

    wind = WindSolver("wind_inputs.i")
    fire = WildfireSolver("fire_inputs.i")

    while fire.time < final_time:
        # Solve wind
        wind.solve()
        vel_3d = wind.get_velocity()
        
        # Update fire wind
        fire.update_wind_3d(vel_3d['u'], vel_3d['v'], vel_3d['w'],
                           wind.nz, wind.zmin, wind.zmax)
        
        # Advance fire
        fire.step()
        
        # Optional: two-way coupling
        state = fire.get_state()
        heat = compute_heat_release(state)
        # wind.add_heat_source(heat)  # Future feature

    wind.finalize()
    fire.finalize()

Key Design Decisions
--------------------

1. Global State Singleton
~~~~~~~~~~~~~~~~~~~~~~~~~~

* Simplifies Python interface (no need to pass C++ objects)
* Matches scientific computing patterns
* Easy cleanup and re-initialization

2. Fortran Order Arrays
~~~~~~~~~~~~~~~~~~~~~~~~

* MultiFab data is naturally column-major (Fortran order)
* Numpy defaults to row-major (C order) for display
* We store in Fortran order and let numpy handle indexing
* Ensures correct data layout for coupled simulations

3. Separation of Concerns
~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``wind_solver_api.cpp``: Core C++ implementation
* ``pyWindSolver.cpp``: pybind11 bindings layer
* ``wind_solver.py``: High-level Python wrapper
* Each layer has clear responsibilities

4. Conservative Data Copies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Data is copied from MultiFab to std::vector to numpy
* Ensures Python owns its data (no lifetime issues)
* Small overhead acceptable for coupling frequency

API Reference
-------------

WindSolver Class
~~~~~~~~~~~~~~~~

**Methods:**

* ``__init__(inputs_file)`` — Initialize solver
* ``solve()`` — Solve for mass-consistent wind
* ``get_velocity()`` — Get 3D velocity dict {'u', 'v', 'w'}
* ``get_velocity0()`` — Get initial (uncorrected) velocity
* ``get_lambda()`` — Get Lagrange multiplier field
* ``get_div0()`` — Get initial divergence field
* ``get_terrain()`` — Get terrain elevation
* ``get_velocity_at_agl(height)`` — Get velocity at AGL height
* ``get_velocity_at_k(k)`` — Get velocity at k-index
* ``update_reference_wind(U_ref, V_ref)`` — Update reference wind
* ``update_parameters(...)`` — Update solver parameters
* ``write_plotfile(name)`` — Write AMReX plotfile
* ``write_extract(filename, agl)`` — Write CSV extract
* ``finalize()`` — Cleanup

**Properties:**

* ``initialized``, ``solved`` — Status flags
* ``nx``, ``ny``, ``nz`` — Grid dimensions
* ``xmin``, ``xmax``, ``ymin``, ``ymax``, ``zmin``, ``zmax`` — Domain bounds
* ``dx``, ``dy``, ``dz`` — Cell sizes
* ``zs_min``, ``zs_max`` — Terrain elevation bounds
* ``iters``, ``residual`` — Last solve statistics

Testing
-------

Run all API tests::

    python3 src/python/test_wind_solver_api.py

Expected output::

    Test 1: Basic initialization - PASSED
    Test 2: Solve and extract - PASSED  
    Test 3: High-level API - PASSED
    Passed: 3/3

Performance Notes
-----------------

* Data extraction is relatively fast (< 1ms for typical grids)
* MLMG solve dominates runtime (seconds for large grids)
* Coupling overhead is negligible compared to solve time
* Recommended coupling frequency: every 1-10 fire timesteps

Planned Enhancements
--------------------

Potential additions for future development:

1. **Time-varying wind**: Support temporal wind evolution
2. **Heat feedback**: Add fire heat source to wind solver
3. **Adaptive parameters**: Auto-tune alpha_h, alpha_v based on conditions
4. **Parallel coupling**: MPI-aware data exchange
5. **Checkpointing**: Save/restore solver state
6. **Visualization**: Built-in plotting utilities

Related Files
-------------

* ``src/wind_solver_api.H`` — C++ API header
* ``src/wind_solver_api.cpp`` — C++ API implementation
* ``src/python/pyWindSolver.cpp`` — pybind11 bindings
* ``src/python/wind_solver.py`` — Python wrapper
* ``src/python/test_wind_solver_api.py`` — Test suite
* ``src/python/coupled_wind_fire_example.py`` — Coupling example
* ``src/python/CMakeLists.txt`` — Python build configuration

References
----------

* **wildfire_levelset PR #230**: Fire solver Python API (reference implementation)
* **AMReX Documentation**: https://amrex-codes.github.io/amrex/
* **pybind11 Documentation**: https://pybind11.readthedocs.io/
