.. _usage:

Usage Guide
===========

This section describes how to configure and run the Mass-Consistent AMR Wind Solver, including details on inputs, parameter settings, advanced physical features (including wind turbine wake models), and complete step-by-step tutorials.

.. contents:: Topics
   :local:
   :depth: 2

Running the Solver
------------------

To run the wind solver, pass an input file to the ``wind_solver`` executable::

    ./build/wind_solver inputs.i

Or supply parameters directly on the command line, which will override values inside the input file::

    ./build/wind_solver terrain_file=terrain.csv U_ref=8.0 z0=0.05

Input File Format
-----------------

The input file uses AMReX ``ParmParse`` syntax — one ``key = value`` pair per line. Lines beginning with ``#`` are comments.

Example ``inputs.i``::

    # Mass-consistent wind solver input file
    terrain_file  = terrain.csv   # X Y Z point cloud
    init_mode     = loglaw        # Wind initialization method
    U_ref         = 8.0           # reference x-wind [m/s]
    V_ref         = 0.0           # reference y-wind [m/s]
    z_ref         = 10.0          # reference height above local terrain [m]
    z0            = 0.05          # aerodynamic roughness length [m]

    dx            = 30.0          # grid spacing x [m]
    dy            = 30.0          # grid spacing y [m]
    dz            = 10.0          # grid spacing z [m]
    domain_height = 300.0         # vertical extent above max terrain [m]

    alpha_h       = 1.0           # horizontal anisotropy coefficient
    alpha_v       = 1.0           # vertical anisotropy coefficient

    mlmg_verbose  = 1             # MLMG verbosity (0=silent, 4=max)
    tol_rel       = 1.e-8         # relative convergence tolerance
    max_grid_size = 32            # max AMReX box size per dimension

    deriv_method  = central       # derivative method (central, weno3, weno5)
    plot_file     = plt_wind      # output plotfile prefix

    extract_agl   = 15.0          # extract CSV at 15 m AGL
    extract_file  = wind_extract.csv

Parameter Reference
-------------------

.. list-table::
   :header-rows: 1
   :widths: 22 15 63

   * - Parameter
     - Default
     - Description
   * - ``terrain_file``
     - ``terrain.csv``
     - Path to terrain point-cloud file (X Y Z, whitespace or comma separated).
   * - **Wind Initialization Mode**
     -
     -
   * - ``init_mode``
     - ``loglaw``
     - Wind field initialization method: ``loglaw`` (log-law profile), ``uniform`` (constant wind), ``raws`` (interpolate from RAWS file), ``surface_data`` (HRRR surface parameters), ``powerlaw`` (power-law profile), ``windfield`` (reads pre-mapped CSV data).
   * - ``U_ref``
     - ``10.0``
     - Reference wind x-component [m/s] at height ``z_ref``.
   * - ``V_ref``
     - ``0.0``
     - Reference wind y-component [m/s] at height ``z_ref``.
   * - ``z_ref``
     - ``10.0``
     - Reference height above the local terrain surface [m].
   * - ``z0``
     - ``0.1``
     - Aerodynamic roughness length [m].
   * - ``powerlaw_exponent``
     - ``0.143``
     - Power-law exponent for ``powerlaw`` mode.
   * - **Grid Parameters**
     -
     -
   * - ``dx``
     - ``30.0``
     - Horizontal grid spacing in x [m].
   * - ``dy``
     - ``30.0``
     - Horizontal grid spacing in y [m].
   * - ``dz``
     - ``30.0``
     - Vertical grid spacing [m].
   * - ``domain_height``
     - ``300.0``
     - Vertical domain extent above the maximum terrain elevation [m].
   * - **Mass-Consistency Parameters**
     -
     -
   * - ``alpha_h``
     - ``1.0``
     - Horizontal Lagrange anisotropy coefficient.
   * - ``alpha_v``
     - ``1.0``
     - Vertical Lagrange anisotropy coefficient (constant).
   * - ``use_height_dependent_alpha_v``
     - ``false``
     - Enable height-dependent vertical anisotropy (linear variation).
   * - ``alpha_v_surface``
     - ``alpha_v``
     - Vertical anisotropy coefficient at ground level (z=z_lo).
   * - ``alpha_v_top``
     - ``alpha_v``
     - Vertical anisotropy coefficient at domain top (z=z_hi).
   * - **MLMG Solver Parameters**
     -
     -
   * - ``mlmg_verbose``
     - ``1``
     - MLMG solver verbosity (0 = silent, 4 = maximum).
   * - ``tol_rel``
     - ``1.0e-8``
     - MLMG relative convergence tolerance.
   * - ``mlmg_max_iter``
     - ``200``
     - Maximum MLMG iterations.
   * - ``mlmg_bottom_solver``
     - ``default``
     - Bottom solver method: ``default``, ``bicgstab``, ``cg``, or ``smoother``.
   * - ``max_grid_size``
     - ``32``
     - Maximum AMReX box size per spatial dimension (increase for GPUs).
   * - **Numerical Methods**
     -
     -
   * - ``deriv_method``
     - ``central``
     - Derivative scheme: ``central`` (2nd order), ``weno3`` (3rd order), or ``weno5`` (5th order).
   * - **Output Parameters**
     -
     -
   * - ``plot_file``
     - ``plt_wind``
     - Output plotfile prefix.
   * - ``extract_agl``
     - ``-1.0``
     - Sample heights AGL [m] for 2-D CSV extraction. Space-separated for multiple heights.
   * - ``extract_file``
     - ``wind_extract.csv``
     - Output filename for terrain-aligned CSV extraction.
   * - **Wind Turbine Wake Parameters**
     -
     -
   * - ``enable_turbine_wake``
     - ``false``
     - Enable analytical turbine wake modeling.
   * - ``turbine_file``
     - (none)
     - Path to turbines CSV layout file.
   * - ``turbine_wake_model_type``
     - ``jensen``
     - Turbine wake model: ``jensen`` (classic linear), ``bastankhah_gaussian`` (Gaussian deficit), or ``turbopark`` (self-similar Gaussian with local TI).
   * - ``turbine_wake_superposition``
     - ``quadratic``
     - Wake deficit superposition method: ``quadratic`` (RSS), ``linear``, or ``max`` (maximum deficit).
   * - ``jensen_kw``
     - ``0.075``
     - Jensen wake decay constant (typically 0.05 for offshore, 0.075 for onshore).
   * - ``gaussian_ka``
     - ``0.05``
     - Bastankhah wake expansion coefficient.
   * - ``turbopark_c1``
     - ``0.38``
     - TurbOPark expansion coefficient.
   * - ``ambient_ti``
     - ``0.075``
     - Ambient turbulence intensity.
   * - ``enable_jimenez_deflection``
     - ``false``
     - Enable Jimenez wake centerline deflection model.
   * - ``jimenez_kd``
     - ``0.05``
     - Jimenez deflection calibration constant.
   * - ``enable_bastankhah_deflection``
     - ``false``
     - Enable Bastankhah & Porté-Agel (2016) wake deflection model.
   * - ``wake_added_turbulence_model``
     - ``none``
     - Analytical wake-added turbulence model: ``none``, ``crespo_hernandez``, or ``frandsen`` (STF).

File Formats
------------

Terrain File Format
~~~~~~~~~~~~~~~~~~~
The terrain CSV file must contain one data point per line with columns **X  Y  Z** (in meters, UTM or local coordinates). Lines beginning with ``#`` are comments::

   # X [m]  Y [m]  Z [m]
   0.0      0.0    5.2
   30.0     0.0    8.1
   ...

Building File Format
~~~~~~~~~~~~~~~~~~~~
Buildings are specified in a CSV file with one building box per line. The optional 7th column specifies building rotation in degrees counter-clockwise from the x-axis::

   # xmin  xmax  ymin  ymax  zmin  zmax  [rotation]
   40.0    60.0  40.0  60.0  0.0   30.0
   100.0   140.0 60.0  80.0  0.0   50.0  45.0

Wind Turbine File Format
~~~~~~~~~~~~~~~~~~~~~~~~
Turbines are defined in a CSV layout file. Columns represent coordinate locations, dimensions, and operational properties in the following order:

1. **x** (required): Easting or local x-coordinate [m].
2. **y** (required): Northing or local y-coordinate [m].
3. **hub_height** (required): Turbine hub height above ground level [m].
4. **rotor_diameter** (required): Rotor diameter [m].
5. **default_ct** (required): Default thrust coefficient.
6. **yaw** (optional): Wake deflection angle relative to incoming wind [degrees].
7. **orientation** (optional): Turbine rotor alignment angle relative to grid x-axis [degrees].
8. **tilt** (optional): Rotor tilt angle [degrees], used to calculate vertical wake deflection.
9. **power_curve_file** (optional): Filename of the CSV or JSON power curve.

**Yaw** defines the active aerodynamic misalignment of the rotor disk relative to the incoming wind direction. This is used to calculate lateral wake deflection and secondary steering. In contrast, **orientation** specifies the fixed absolute physical heading of the rotor face relative to the grid coordinate system. **Tilt** defines the backward/upward rotation of the rotor disk around the crosswind axis, used to model vertical wake deflection.

Example::

   # x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, tilt, power_curve_file
   100.0, 200.0, 90.0, 120.0, 0.8, 15.0, 45.0, 10.0, test_turbine.json

Power Curve Format (CSV/JSON)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Discrete electrical power outputs and thrust coefficients are mapped in an optional power curve file, which can be either a flat CSV or a standard JSON specification file (facilitating interoperability with FLORIS and PyWake):

* **CSV Format**::

     # wind_speed, power_kw, ct
     3.0, 0.0, 0.8
     5.0, 1000.0, 0.78
     10.0, 5000.0, 0.5
     25.0, 5000.0, 0.1

* **JSON Format** (FLORIS and PyWake compatible)::

     {
       "power_thrust_table": {
         "wind_speed": [3.0, 5.0, 10.0, 25.0],
         "power": [0.0, 1000.0, 5000.0, 5000.0],
         "thrust_coefficient": [0.8, 0.78, 0.5, 0.1]
       }
     }

3D Meteorological Ingestion (NetCDF)
------------------------------------

The C++ solver can ingest pre-processed 3D wind fields from larger-scale weather prediction models (such as WRF or GFS outputs) through the `tools/netcdf_to_windfield.py` utility.

1. **Interpolation and Parsing**:
   
   .. code-block:: bash

      python3 tools/netcdf_to_windfield.py \
        --nc-files wrf_t1.nc wrf_t2.nc \
        --inputs inputs.i \
        --output windfield.csv \
        --time 50.0

2. **Solver Configuration**:
   Configure the solver in ``inputs.i`` to load this wind field:

   .. code-block:: ini

      init_mode = windfield
      windfield_file = windfield.csv

FLORIS Integration
------------------

The solver can export wind speeds in formats compatible with NREL's FLORIS (Wind Farm Simulation Software) without requiring a local FLORIS installation:

.. code-block:: bash

    python3 tools/floris_export.py \
        --solver inputs.i \
        --turbines turbines.csv \
        --hub-height 90.0 \
        --reference-speed 10.0 \
        --output wind_data.csv

Wind Farm & Coupling Integration
--------------------------------

PyWake Integration and Site Export
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The solver integrates directly with DTU's PyWake wind farm simulation library. This integration allows users to format and extract mass-consistent wind fields as native PyWake ``Site`` or ``WAsPGridSite`` structures. Additionally, grid maps of terrain, roughness, wind speed, and wind direction can be exported to Surfer ASCII ``.grd`` formats compatible with WAsP.

.. code-block:: python

    import sys
    from wind_solver import WindSolver
    from pywake_coupling import MassConsistentSite, to_wasp_grid_site

    # 1. Initialize and execute the mass-consistent wind solver
    wind = WindSolver("inputs.i")
    wind.solve()

    # 2. Extract resolved fields as a standard PyWake Site object
    site = MassConsistentSite(wind)
    
    # Query local wind properties at multiple coordinates and heights
    local_wind = site.local_wind(x=[100.0, 500.0], y=[200.0, 200.0], h=[90.0, 90.0])
    print("Local wind directions:", local_wind.WD_ilk)
    print("Local wind speeds:", local_wind.WS_ilk)

    # 3. Export resolved wind fields to WAsP Surfer GRD files and instantiate a WAsPGridSite
    wasp_site = to_wasp_grid_site(wind, height_agl=90.0, output_dir="wasp_grids")
    print("WAsP Surfer GRD files successfully generated in 'wasp_grids/'")

    # 4. Clean up resources
    wind.finalize()

Wind Turbine Yaw Setup & Wake Steering
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To configure wake deflection and secondary steering via yaw, specify the active wake parameterizations in ``inputs.i`` and define specific yaw angles in ``turbines.csv``:

1. **Enable models in inputs.i**:

   .. code-block:: ini

       enable_turbine_wake = true
       turbine_file = turbines.csv
       turbine_wake_model_type = gch            # Gauss-Curl Hybrid (secondary steering)
       enable_jimenez_deflection = false        # Jimenez model for yawed turbines
       enable_bastankhah_deflection = true      # Bastankhah & Porté-Agel (2016) deflection model
       jimenez_kd = 0.05                        # Jimenez deflection decay rate
       wake_added_turbulence_model = frandsen   # Wake-added turbulence model

2. **Specify layout and yaw values in turbines.csv**:

   .. code-block:: csv

       # x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, [power_curve_file]
       200.0, 500.0, 80.0, 110.0, 0.8, 20.0, 0.0, nrel_5mw.csv
       600.0, 500.0, 80.0, 110.0, 0.8, 0.0, 0.0, nrel_5mw.csv

Coupling Integration with External Solvers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Programmatic coupling allows passing dynamic 3D wind velocity arrays to external solvers (e.g., wild-land fire spread solvers such as ``wildfire_levelset``) in a tight-coupling framework.

.. code-block:: python

    import time
    from wind_solver import WindSolver
    from wildfire_solver import WildfireSolver

    # Initialize the wind and fire solvers
    wind = WindSolver("wind_inputs.i")
    fire = WildfireSolver("fire_inputs.i")

    # Coupling loop
    step = 0
    while fire.time < 3600.0:
        # Step A: Update and solve mass-consistent wind field
        wind.solve()
        vel_3d = wind.get_velocity()
        
        # Step B: Pass 3D wind arrays (nz, ny, nx) to the fire spread solver
        # The fire solver maps these onto its internal grid using vertical interpolation
        fire.update_wind_3d(
            vel_3d['u'], 
            vel_3d['v'], 
            vel_3d['w'],
            wind.nz, 
            wind.zmin, 
            wind.zmax
        )

        # Step C: Advance the wildfire levelset front
        fire.step()
        print(f"Step {step}: advanced fire simulation time to {fire.time:.1f} s")
        step += 1

    wind.finalize()
    fire.finalize()

Step-by-Step Walkthrough Tutorials
----------------------------------

Walkthrough Section 1: Baseline Wind Solver & Terrain Setup
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This walkthrough demonstrates configuring a baseline mass-consistent wind solve over synthetic Gaussian topography.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    # Terrain configuration
    terrain_file = terrain.csv          # Point-cloud file

    # Wind profile initialization
    init_mode    = loglaw               # Log-law initialization
    U_ref        = 15.0                 # Reference wind x-component [m/s]
    V_ref        = 0.0                  # Reference wind y-component [m/s]
    z_ref        = 10.0                 # Reference height AGL [m]
    z0           = 0.03                 # Aerodynamic roughness length [m]

    # Computational grid spacing [m]
    dx           = 25.0
    dy           = 25.0
    dz           = 20.0

    # Domain vertical height [m]
    domain_height = 200.0               # Height above maximum terrain peak

    # Mass-consistency parameters
    alpha_h      = 1.0                  # Horizontal penalty coefficient
    alpha_v      = 1.0                  # Vertical penalty coefficient

    # Extraction and output
    plot_file    = plt_case1_output     # Output plotfile prefix
    extract_agl  = 30.0                 # Extract wind velocity at 30m AGL
    extract_file = wind_extract.csv

Run this baseline scenario located at ``test/mass_consistent_case1_gaussian_hill/``::

    python3 test_case1.py

Walkthrough Section 2: Advanced Solver & Boundary Layer Dynamics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This tutorial covers height-dependent anisotropy, Ekman spiral veer correction, buoyancy effects, and high-order derivative schemes (WENO-5).

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    terrain_file  = terrain.csv
    init_mode     = loglaw
    U_ref         = 10.0
    V_ref         = -5.0
    z_ref         = 10.0
    z0            = 0.05

    dx            = 50.0
    dy            = 50.0
    dz            = 30.0
    domain_height = 300.0

    # Height-Dependent Anisotropy
    use_height_dependent_alpha_v = true
    alpha_v_surface              = 1.5
    alpha_v_top                  = 0.5
    alpha_h                      = 1.0

    # Ekman Spiral Veer
    enable_ekman_veer            = true
    latitude                     = 45.3
    ekman_veer_total             = 25.0
    ekman_veer_height            = 200.0

    # Buoyancy Stratification
    enable_buoyancy_stratification = true
    temperature_file             = temperature.csv
    temperature_reference        = 285.0
    buoyancy_coefficient         = 1.2
    buoyancy_method              = rhs

    # Numerical Methods
    deriv_method                 = weno5
    tol_rel                      = 1.e-9
    mlmg_max_iter                = 300
    mlmg_bottom_solver           = bicgstab

    plot_file                    = plt_advanced_bl

Walkthrough Section 3: Vegetation & Forest Canopy Modeling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This section configures forest canopies using MacDonald top-canopy adjustments and Shaw-Pereira exponential decay inside porous canopy layers.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    terrain_file       = terrain.csv
    init_mode          = loglaw
    U_ref              = 10.0
    V_ref              = 0.0
    z_ref              = 50.0
    z0                 = 0.05

    dx                 = 50.0
    dy                 = 50.0
    dz                 = 5.0

    domain_height      = 200.0

    # Forest Canopy Parameters
    enable_canopy            = true
    canopy_height            = 20.0
    frontal_area_index       = 0.25
    plan_area_index          = 0.20
    canopy_drag_coeff        = 0.2
    use_exponential_profile  = true
    canopy_attenuation       = 2.5

    plot_file                = plt_canopy_forest

Walkthrough Section 4: Atmospheric Stability and Slope Flows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This tutorial configures non-neutral atmospheric boundary layers and slope-driven thermal flows.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    # Enable stability corrections
    enable_stability_correction = true
    stability_length            = -150.0            # Unstable convective layer

    # Thermally-driven slope flows
    enable_katabatic_flow       = true              # Enable katabatic parameterization
    slope_flow_u_max            = 3.5               # Max slope flow velocity [m/s]
    slope_flow_z_max            = 15.0              # Height of peak velocity [m/s]

Walkthrough Section 5: Passive Gaussian Puff Dispersion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This tutorial demonstrates coupling wind transport with Gaussian pollutant dispersion, first-order chemical decay, and deposition.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    enable_puff              = true
    source_x                 = 150.0
    source_y                 = 150.0
    source_z                 = 10.0
    emission_rate            = 1.0
    emission_duration        = 50.0

    K_h                      = 1.0
    K_v                      = 0.5
    sigma_y0                 = 1.0
    sigma_z0                 = 1.0

    enable_decay             = true
    decay_constant           = 0.001

    enable_puff_deposition   = true
    deposition_velocity      = 0.01

Run the standalone puff solver::

    ./build/puff_solver regtest/puff_gaussian/inputs.i

Walkthrough Section 6: Synthetic Turbulence & BTS Generation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This walkthrough generates terrain-aware synthetic turbulence fluctuations and exports them in OpenFAST-compatible binary format.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    enable_synthetic_turbulence    = true
    turbulence_spectrum_model      = VonKarman
    turbulence_intensity_model     = PowerLaw
    turbulence_coherence_model     = Gaussian
    turbulence_intensity_ref       = 0.12
    turbulence_length_scale_u      = 300.0
    turbulence_length_scale_v      = 200.0
    turbulence_length_scale_w      = 120.0
    turbulence_random_seed         = 12345
    turbulence_export_format       = bts
    turbulence_output_file         = turbulence.bts

Performance Tuning
------------------

MLMG Multigrid Optimization
~~~~~~~~~~~~~~~~~~~~~~~~~~~
For flat terrain, reduce V-cycle iterations per step::

    mlmg_max_iter = 100
    mlmg_pre_smooth = 8
    mlmg_post_smooth = 8

GPU Performance
~~~~~~~~~~~~~~~
When using CUDA, HIP, or SYCL, increase the box size per GPU rank to maximize occupancy::

    max_grid_size = 128
