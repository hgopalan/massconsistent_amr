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

**Factory classmethods**:

* ``from_scm(terrain_file, U_ref, dir_ref, z_ref, L_obukhov, latitude, z0, T_ref, z_T_ref, lapse_rate, dt, max_time, conv_tol, nx, ny, nz, dx, dy, dz, extra_params)``:
  Creates and initialises a ``WindSolver`` instance using the Single Column Model
  (``init_mode = scm``).  Writes a temporary inputs file, calls ``initialize()``,
  and returns the ready-to-solve instance.  All keyword arguments correspond to
  the ``scm.*`` ParmParse parameters; see :ref:`scm_parameters` in the main usage
  documentation.

  .. code-block:: python

      from wind_solver import WindSolver

      # Neutral SCM on flat terrain at 45° N
      wind = WindSolver.from_scm(
          terrain_file = "terrain.csv",
          U_ref        = 10.0,     # reference wind speed [m/s]
          dir_ref      = 270.0,    # westerly
          z_ref        = 10.0,
          z0           = 0.1,
          L_obukhov    = 1.0e6,    # near-neutral
          latitude     = 45.0,
          T_ref        = 300.0,
          nx=20, ny=20, nz=40,
          dx=50.0, dy=50.0, dz=50.0,
      )
      wind.solve()

**Methods**:

* ``solve()``: Triggers the mass-consistent Poisson adjustment.
* ``get_velocity()``: Returns a dictionary with keys ``'u', 'v', 'w'`` containing 3D velocity components.
* ``get_velocity_at_agl(height)``: Extracts the 2D horizontal plane of velocity at a given height above ground level (AGL). Returns a dictionary of 2D NumPy arrays.
* ``get_terrain()``: Returns a 2D NumPy array representing the interpolated terrain elevation.
* ``get_scm_profiles()``: Returns the 1D SCM column profiles produced during ``init_mode = scm`` initialization as a dict of NumPy arrays:

  .. code-block:: python

      profiles = wind.get_scm_profiles()
      # profiles keys: 'z', 'u', 'v', 'theta', 'tke', 'Km', 'Kh'
      # Each array has shape (1000,) corresponding to the 4000 m / 4 m column.

      import matplotlib.pyplot as plt
      plt.plot(profiles['u'], profiles['z'], label='u [m/s]')
      plt.plot(profiles['v'], profiles['z'], label='v [m/s]')
      plt.xlabel('Wind component [m/s]')
      plt.ylabel('Height AGL [m]')
      plt.legend()
      plt.show()

  Raises ``RuntimeError`` if the solver was not initialized with ``init_mode = scm``.

* ``update_reference_wind(U_ref, V_ref)``: Re-evaluates friction velocities and updates reference flow parameters.
* ``write_plotfile(name)``: Writes the standard MultiFab cell-centered outputs in VisIt/ParaView compatible AMReX plotfile format.
* ``finalize()``: Destroys the C++ state singleton and cleans up AMReX runtime resources.

Building Wake Enhancement Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Building wake enhancements can be controlled directly through the inputs file passed to the ``WindSolver`` constructor. All enhancements are optional and backward compatible. The following parameters enable or disable fourteen advanced wake modeling features:

**Global Flags** (enable/disable enhancements):

.. code-block:: python

    # In the inputs.i file passed to WindSolver:
    enable_oblique_scaling = true              # Scale cavity by wind approach angle
    enable_tall_building_correction = true     # Aspect-ratio correction for tall buildings
    enable_gaussian_profile = false            # Gaussian vs. linear lateral profile
    enable_upwind_recirculation = true         # Model reverse flow upstream
    enable_reference_correction = false        # Log-law reference velocity extraction
    enable_corner_acceleration = true          # Corner and side flow acceleration
    enable_variance_correction = false         # Height-dependent velocity variance
    enable_horseshoe_vortex = true             # Horseshoe vortex at building base
    enable_extended_farwake = true             # Extend far-wake to 15H
    enable_yoshie_two_layer = true             # Yoshie height-dependent deficit model
    enable_rodi_entrainment = true             # Rodi entrainment-based far-wake decay
    enable_lopes_comfort = true                # Pedestrian wind comfort assessment
    enable_oikonomou_aspect = true             # Aspect-ratio dependent cavity correction
    enable_britter_hanna_urban = true          # Urban canyon wind speed attenuation

**Model-Specific Parameters**:

.. code-block:: python

    # Yoshie two-layer model
    yoshie_decay_beta = 1.75                   # Exponential decay coefficient (1.5-2.0)

    # Rodi entrainment model
    rodi_ce_coefficient = 1.0                  # Entrainment coefficient (0.5-1.5)

    # Lopes pedestrian comfort assessment
    lopes_comfort_threshold = 5.0              # Critical velocity (m/s)
    lopes_assessment_height = 1.5              # Assessment height above ground (m AGL)
    lopes_reference_frequency = 0.02           # Reference discomfort frequency

    # Oikonomou aspect-ratio correction
    oikonomou_beta_aspect = 0.25               # Aspect-ratio correction coefficient (0.15-0.35)

    # Britter-Hanna urban canyon attenuation
    britter_hanna_alpha = 0.15                 # Attenuation coefficient (0.1-0.3)

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

FLORIS Coupling and Export
--------------------------

The ``floris_coupling`` module provides tools to interpolate and export mass-consistent wind fields into formats compatible with NREL's FLORIS (Wind Farm Simulation Software). No FLORIS installation is required for data generation or export.

Two Integration Modes
~~~~~~~~~~~~~~~~~~~~~

FLORIS can be integrated with the mass-consistent wind solver in two distinct ways:

1. **Stand-alone Mode (Offline Data Ingestion)**: Run the mass-consistent wind solver beforehand, interpolate the wind field at turbine hub heights, and export it to a standardized CSV or JSON file. The exported file is then ingested by FLORIS offline using its static data loaders (such as the `TimeSeriesInterface`), without requiring `massconsistent_amr` to run concurrently during wind farm wake optimization.
2. **Directly Coupled Mode (Memory-Resident via Python API)**: Run the wind solver programmatically inside a unified Python optimization loop using `pywindsolver`. Wind speed and direction profiles are extracted dynamically and passed directly in-memory into FLORIS's `FlorisInterface`, enabling real-time, closed-loop wind farm yaw control and layout optimization.

Mode 1: Stand-alone Mode (Offline Data Ingestion)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this mode, you generate the wind data from `massconsistent_amr` and write it to disk. No FLORIS installation is required to generate or export this data.

**Step 1: Export wind speeds using the solver**

.. code-block:: python

    from wind_solver import WindSolver
    from floris_coupling import FLORISWindMap, quick_export

    # Initialize and solve the terrain-following wind field
    wind = WindSolver("inputs.i")
    wind.solve()

    # Create a FLORIS-compatible Wind Map and define turbine layouts
    wind_map = FLORISWindMap(wind)
    turbines = [(100.0, 200.0), (300.0, 400.0), (500.0, 400.0)]

    # Export wind velocities at turbine hub-height (e.g., 90m) to CSV
    wind_map.export_to_csv(turbines, hub_height=90.0, output_file="floris_wind_data.csv")
    wind.finalize()

**Step 2: Ingest the exported CSV inside your FLORIS script**

Inside your independent FLORIS script, load the exported CSV to configure the local environmental conditions for your simulation:

.. code-block:: python

    import pandas as pd
    from floris.tools import FlorisInterface

    # Load the mass-consistent wind data exported earlier
    df = pd.read_csv("floris_wind_data.csv")

    # Initialize FLORIS with your wind farm configuration
    fi = FlorisInterface("gch.yaml")

    # Assign mass-consistent wind speeds and directions to FLORIS
    fi.reinitialize(
        wind_speeds=df["speed_ms"].values,
        wind_directions=df["direction_deg"].values,
    )

    # Run the wake model and calculate AEP
    fi.calculate_wake()
    power = fi.get_turbine_powers()

Mode 2: Directly Coupled Mode (Memory-Resident via Python API)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This mode integrates the mass-consistent wind solver (`pywindsolver` via the `wind_solver` Python wrapper) directly with FLORIS's active `FlorisInterface` in-memory. This is ideal for active flow control, yaw optimization sweeps, and dynamic site-specific modeling.

.. code-block:: python

    from wind_solver import WindSolver
    from floris_coupling import FLORISWindMap
    from floris.tools import FlorisInterface
    import numpy as np

    # 1. Initialize and run the mass-consistent wind solver
    wind = WindSolver("inputs.i")
    wind.solve()
    wind_map = FLORISWindMap(wind)

    # 2. Instantiate the FLORIS Interface
    fi = FlorisInterface("gch.yaml")
    
    # Extract coordinates of turbines defined in FLORIS
    layout_x, layout_y = fi.layout_x, fi.layout_y
    turbines = list(zip(layout_x, layout_y))

    # 3. Retrieve local, terrain-steering wind conditions dynamically in-memory
    winds = wind_map.get_wind_at_turbines(turbines, hub_height=90.0)
    
    speeds = np.array([w['speed'] for w in winds])
    directions = np.array([w['direction'] for w in winds])

    # 4. Programmatically pass the terrain-resolved wind profiles into FLORIS in-memory
    fi.reinitialize(
        wind_speeds=speeds,
        wind_directions=directions,
    )

    # 5. Compute wake interactions and turbine performance
    fi.calculate_wake()
    power_outputs = fi.get_turbine_powers()

    # Clean up C++ solver resources
    wind.finalize()

FLORISWindMap Class Reference
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Constructor**:
* ``__init__(wind_solver)``: Initializes the FLORISWindMap with an active and solved ``WindSolver`` instance.

**Methods**:
* ``get_wind_at_point(x, y, z)``: Tri-linearly interpolates the 3D wind velocity vector to a specific location, returning a dictionary containing velocity components (``'u', 'v'``), wind speed (``'speed'``), and meteorological wind direction (``'direction'``).
* ``get_wind_at_turbine(turbine_x, turbine_y, hub_height)``: Extracts terrain-aware wind properties at a turbine's hub height AGL.
* ``get_wind_at_turbines(turbine_locations, hub_height)``: Performs batch interpolation for a list of turbine positions.
* ``export_to_csv(turbine_locations, hub_height, output_file, reference_speed=None)``: Writes interpolated turbine wind properties to a CSV file. If ``reference_speed`` is specified, includes the speed-up ratio.
* ``export_to_json(turbine_locations, hub_height, output_file, reference_speed=None)``: Writes a structured JSON file containing solver metadata, extraction settings, and per-turbine wind velocity/elevation profiles.
* ``export_to_dict(turbine_locations, hub_height, reference_speed=None)``: Returns a nested Python dictionary containing the interpolated wind map metadata and turbine-by-turbine states.
* ``get_speed_map_2d(height)``: Extracts a full 2D grid of terrain-aligned wind speeds at a specified height above ground level (AGL). Returns a tuple of ``(speed_map, x_coords, y_coords)``.

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

Agricultural Drone Spraying & Operational Optimization
------------------------------------------------------

The ``agricultural_drone`` module coordinates drone flight trajectory parsing, nozzle mass flow regulation, 3D analytical rotor downwash velocity mapping, and multi-compartment dry deposition/canopy interception modeling. It provides both moving-source Gaussian Puff and Lagrangian Particle Dispersion Model (LPDM) simulation loops, coupled directly with the C++ ``WindSolver`` for realistic wind field advection.

Basic Usage
~~~~~~~~~~~
To parse a drone flight trajectory, configure nozzle droplet distributions, and execute an LPDM simulation over a forested canopy field:

.. code-block:: python

    from agricultural_drone import (
        DroneTrajectory, MassEmissionRegulator, DroneLpdDispersion
    )
    from wind_solver import WindSolver

    # 1. Load flight trajectory from telemetry CSV
    trajectory = DroneTrajectory.from_csv("telemetry.csv")

    # 2. Configure Mass Flow Regulator with droplet size bins
    droplet_bins = {
        'fine': {'diameter': 50e-6, 'fraction': 0.20},
        'medium': {'diameter': 150e-6, 'fraction': 0.50},
        'coarse': {'diameter': 350e-6, 'fraction': 0.30}
    }
    regulator = MassEmissionRegulator(
        formulation_density=1000.0,  # g/L (water-like)
        active_fraction=0.1,         # 10% active chemical ingredient
        droplet_bins=droplet_bins
    )

    # 3. Initialize mass-consistent C++ WindSolver
    wind = WindSolver("inputs.i")
    wind.solve()

    # 4. Create and configure LPD Dispersion Model matching the C++ grid
    dispersion = DroneLpdDispersion()
    dispersion.setup_grid_from_solver(wind)

    # 5. Execute simulation loop along flight timeline
    dispersion.simulate(
        trajectory=trajectory,
        regulator=regulator,
        wind_solver=wind,
        dt=0.5,
        particles_per_step=80,
        enable_settling=True,
        enable_evaporation=True,
        enable_canopy_interception=True,
        canopy_height=2.0,
        leaf_area_index=3.0,
        frontal_area_index=1.0
    )

    # 6. Validate mass conservation and extract deposition registers
    conserved, balance = dispersion.verify_mass_conservation()
    if conserved:
        print("Pesticide mass is perfectly conserved across all registers!")
        print(f"  Ground Deposition: {dispersion.ground_deposition.sum():.2f} g")
        print(f"  Canopy Deposition: {dispersion.canopy_top_deposition.sum():.2f} g")

    wind.finalize()

Walkthrough of the Simulation Process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The full pesticide spraying simulation executes through five highly-coordinated phases:

1. **Trajectory Parsing and Interpolation**:
   The ``DroneTrajectory`` class parses discrete flight logs (CSV or arrays of ``time, x, y, z, speed, heading, flow_rate, active``) and performs linear interpolation of all telemetry fields at any arbitrary intermediate simulation timestamp ``t``.
2. **Mass Flow Regulation**:
   The ``MassEmissionRegulator`` parses the nozzle's volumetric flow rate (L/min) and converts it to active pesticide mass emission (g/s) based on chemical formulation density and active concentration fractions. If speed-dependent scaling is enabled, flow rate is adjusted dynamically to ensure constant ground coverage density under varying flight velocities.
3. ** Droplet Transport & Downwash Integration**:
   Released droplets are binned into size fractions ('fine', 'medium', 'coarse') and advected under the combined influence of the 3D wind velocity field (interpolated from C++ ``WindSolver``) and 3D analytical rotor downwash velocity. Radial spreading and wall-jet ground interaction are fully modeled, along with size-dependent Stokes settling velocity (incorporating the Cunningham slip correction).
4. **Physicochemical Evolution**:
   Airborne droplets shrink dynamically due to evaporation (calculated using the Tetens saturation vapor pressure equation and d² evaporation law). The active pesticide chemical degrades exponentially according to ambient temperature and photolysis (solar radiation) half-life references.
5. **Crop Canopy Interception & Deposition Mapping**:
   Spatially distributed forest/crop canopy properties (height, Leaf Area Index, Frontal Area Index) are mapped to 2D arrays. Descending droplets are intercepted by the foliage layer using empirical deposition velocity models. Deposits are recorded into canopy-top, lower-foliage, and ground registers, allowing complete mass conservation validation.

.. figure:: drone_deposition_plot.png
   :width: 100%
   :align: center
   :alt: Agricultural Drone Deposition & Drift

   *Figure: 2D visualization of crop deposition and off-target spray drift under crosswind.*

Validation, Sensitivity Analysis & Operational Optimization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Two production-grade automated tools are provided under the ``tools/`` directory:

- **Sensitivity Analysis (``drone_sensitivity_analysis.py``)**:
  Runs systematic multi-parameter sweeps to evaluate how pesticide spray drift and deposition efficiency respond to:
  * *Nozzle diameter* (generating finer or coarser droplet bins).
  * *Flight altitude* (altering advection transit time).
  * *Wind speed* (driving off-target advective drift).
  * *Atmospheric stability* (controlling horizontal and vertical eddy mixing diffusivities).
- **Weather Window Optimizer (``weather_window_optimizer.py``)**:
  Designs a safe operational meteorology envelope. It runs batch spraying simulations against the cached ``ScenarioLibrary`` to identify:
  * *Maximum allowable wind speeds* for fine, medium, and coarse nozzles.
  * *Optimal spraying times of day* (Early Morning, Mid-day, Afternoon, Night) based on diurnal temperature, humidity, wind, and stability profiles.

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
