.. _external_coupling:

External Coupling
=================

This section describes the interfaces, python couplings, and mathematical frameworks for coupling the Mass-Consistent AMR Wind Solver with external physics solvers.

PHREEQC Coupling
----------------

Overview
~~~~~~~~

The PHREEQC coupling framework provides one-way integration with geochemical reactive transport solvers for wind-driven studies, such as critical mineral leaching, acid mine drainage (AMD) analysis, and contaminant transport with terrain-resolved atmospheric boundary conditions.

The coupling code is located in: ``tests_and_examples/phreeqc_coupling/``

The directory contains 11 standalone example scripts demonstrating core capabilities:

1. **01_wind_field_bc.py** — Wind velocity as boundary condition for pore-water advection
2. **02_temperature_profile_bc.py** — Temperature profile extraction from wind solver
3. **03_precipitation_recharge.py** — Infiltration mapping and recharge calculations
4. **04_kv_dispersivity.py** — Vertical permeability and dispersivity extraction
5. **05_stability_classification.py** — Pasquill-Gifford-Turner stability classification
6. **06_valley_amd_hotspots.py** — Acid mine drainage hotspot detection in valleys
7. **07_sulfide_oxidation.py** — Oxidation kinetics for sulfide minerals
8. **08_spatial_temperature_cache.py** — Scenario caching for rapid deployments
9. **09_dust_suppression.py** — Dust settling and suppression calculations
10. **10_leaching_efficiency_sherwood.py** — Leaching enhancement via Sherwood number
11. **11_end_to_end_facility.py** — Complete workflow demonstration

Supported Features
~~~~~~~~~~~~~~~~~~

- **Wind velocity boundary conditions** for subsurface flow modeling.
- **Temperature profile extraction** at arbitrary heights.
- **Precipitation and recharge mapping** over complex topography.
- **Vertical permeability and dispersivity estimation**.
- **Atmospheric stability classification** (Pasquill-Gifford-Turner).
- **Acid mine drainage (AMD) hotspot detection** in valley environments.
- **Sulfide oxidation kinetics** in ore/tailings piles.
- **Leaching efficiency enhancement** calculation via Sherwood number.
- **Fine dust suppression** calculations via settling velocities.
- **Spatial temperature caching** for rapid scenario evaluation.

Production Readiness:
- **Status:** PRODUCTION-READY
- **Confidence Level:** HIGH for trend predictions, MODERATE for absolute rates.

Python Tools
~~~~~~~~~~~~

The Python tools and extractors expose interfaces to extract velocity, stability, and atmospheric state variables from the wind solver and pass them to PHREEQC.

1. Field Extractor (Wind & Temperature boundary conditions)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

    from wind_solver import WindSolver
    from phreeqc_coupling import FieldExtractor

    wind = WindSolver("inputs.i")
    wind.solve()

    extractor = FieldExtractor(wind)
    z_agl, T_profile = extractor.export_temperature_profile()

2. Acid Mine Drainage Hotspot Detection
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

    from phreeqc_coupling.amd_hotspot_detector import identify_valley_amd_hotspots

    results = identify_valley_amd_hotspots(
        wind_field=u_field,
        slope_field=slope_field,
        threshold_angle=15.0
    )

3. Sulfide Oxidation Kinetics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

    from phreeqc_coupling.sulfide_oxidation import compute_sulfide_oxidation_rates

    rates = compute_sulfide_oxidation_rates(
        temperature_profile=T_profile,
        oxygen_fraction=0.21,
        pyrite_fraction=0.05
    )

4. Scenario Library Caching
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

    from phreeqc_coupling.scenario_library import build_scenario_library, ScenarioLibrary

    # Build and cache scenarios
    lib = build_scenario_library(n_scenarios=100, output_dir='scenarios/')

    # Quick runtime lookup (< 30 seconds)
    loaded_lib = ScenarioLibrary.load('scenarios/library.h5')
    scenario = loaded_lib.nearest_scenario(wind_speed=12.0, wind_dir=270.0)

5. Dust Suppression & Settling Calculations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

    from phreeqc_coupling.dust_suppression_lookup import compute_dust_suppression_factor

    factor = compute_dust_suppression_factor(
        wind_speed=8.5,
        particle_size_microns=10.0
    )

6. Leaching Efficiency via Sherwood Number
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

    from phreeqc_coupling.leaching_efficiency import compute_leaching_efficiency

    efficiency = compute_leaching_efficiency(
        wind_speed=3.5,
        particle_diameter_microns=500.0
    )

References for PHREEQC part
~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **Parkhurst, D.L., & Appelo, C.A.J.** (2013). Description of the PHREEQC (Version 3) computer program for speciation, batch-reaction, one-dimensional transport, and inverse geochemical calculations. *USGS Techniques and Methods*, Book 6, Chapter A43.
- **Nicholson, R.V., Gillham, R.W., & Reardon, E.J.** (1990). Pyrite oxidation in carbonate-buffered systems. *Geochimica et Cosmochimica Acta*, 54(2), 395–405.
- **Stumm, W., & Morgan, J.J.** (1996). *Aquatic Chemistry* (3rd ed.). Wiley-Interscience.
- **Plummer, L.N., & Busenberg, E.** (1982). The solubility of calcite, aragonite and vaterite in CO₂-H₂O solutions. *Geochimica et Cosmochimica Acta*, 46(6), 1011–1040.
- **Sherwood, T.K.** (1954). Mass transfer between phases. *Industrial & Engineering Chemistry*, 46(2), 221–231.
- **Ranz, W.E., & Marshall, W.R.** (1952). Evaporation from drops. *Chemical Engineering Progress*, 48(3), 141–146.
- **Gelhar, L.W., Welty, C., & Rehfeldt, K.R.** (1992). A critical review of data on field-scale dispersion in aquifers. *Water Resources Research*, 28(7), 1955–1974.

Wildfire
--------

The `wildfire_levelset <https://github.com/hgopalan/wildfire_levelset>`_ framework provides AMReX-based wildfire front propagation capabilities.

Python API for Coupled Simulations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Python API (``pyWildfire``) provides complete programmatic control of the wildfire solver from Python, enabling coupled wind-fire simulations with external wind solvers.

Key capabilities:
- Initialize fire solver from inputs file.
- Time-step the fire simulation.
- Extract fire fields (level-set phi, rate of spread, intensity, flame length, wind components) as NumPy arrays.
- Update wind fields from 2D or 3D arrays.
- Write AMReX plotfiles.
- Zero-copy data transfer.

Building with Python Bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To enable Python bindings during CMake configuration:

.. code-block:: bash

   cmake -S . -B build \
     -DLEVELSET_DIM_2D=ON \
     -DLEVELSET_BUILD_PYTHON_BINDINGS=ON
   cmake --build build -j

Set the Python path:

.. code-block:: bash

   export PYTHONPATH=$PWD/build/python:$PYTHONPATH

Python Quick Start
~~~~~~~~~~~~~~~~~~

Basic fire simulation workflow:

.. code-block:: python

   from wildfire_solver import WildfireSolver
   
   # Initialize
   fire = WildfireSolver("inputs.i")
   
   # Run simulation
   for i in range(100):
       fire.step()
       state = fire.get_state()
       burned_area = (state['phi'] <= 0).sum() * fire.dx * fire.dy
       print(f"t={state['time']:.1f}s, burned={burned_area:.0f}m²")
   
   # Finalize
   fire.finalize()

Coupled Wind-Fire Simulations (Ember Coupling)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Python API enables coupling between `massconsistent_amr` and `pyWildfire` solvers to capture dynamic feedback:

.. code-block:: python

   from wildfire_solver import WildfireSolver
   from pyWindSolver import WindSolver  # massconsistent_amr module
   
   # Initialize both solvers
   fire = WildfireSolver("fire_inputs.i")
   wind = WindSolver("wind_inputs.txt")
   
   # Coupled time loop
   final_time = 3600.0  # 1 hour
   
   while fire.time < final_time:
       # 1. Solve wind field
       wind.solve(fire.time)
       u_3d, v_3d, w_3d = wind.get_velocity_arrays()
       
       # 2. Update fire wind
       fire.update_wind_3d(u_3d, v_3d, w_3d, wind.nz, wind.zmin, wind.zmax)
       
       # 3. Advance fire
       fire.step()
       
       # 4. Extract state
       state = fire.get_state()
       burned = (state['phi'] <= 0).sum() * fire.dx * fire.dy
       print(f"t={fire.time:.1f}s, burned={burned:.0f}m²")
   
   # Finalize
   fire.finalize()
   wind.finalize()


Agricultural Drone Operations & Pest Management
-----------------------------------------------

Overview
~~~~~~~~

The Agricultural Drone Operations & Pest Management module provides specialized modeling for agricultural drone pesticide application, spray drift, and crop canopy deposition. Drones spraying liquid pesticides, herbicides, or biological agents operate close to complex terrain and vegetation in the atmospheric boundary layer.

This module supports:
- **Drone Path Representation**: Loading and interpolating multi-variable flight telemetry (3D coordinates, speed, heading, flow rates).
- **Dynamic Point Sources**: Simulating moving Lagrangian particles (LPDM) and Gaussian Puffs originating from translating spray nozzles.
- **Mass Emission Regulation**: Converting volumetric nozzle flow rates to physical pesticide active ingredient mass flow with velocity-dependent scaling.

Mathematical Model
~~~~~~~~~~~~~~~~~~

Drone Trajectory & Interpolation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A flight trajectory is defined by discrete state vectors :math:`\mathbf{S}_n = (t_n, x_n, y_n, z_n, U_n, \theta_n, Q_n, a_n)`, where :math:`U_n` is flight speed, :math:`\theta_n` is drone heading, :math:`Q_n` is the volumetric nozzle flow rate, and :math:`a_n` is the binary active spray flag.

For any arbitrary time :math:`t_n \le t \le t_{n+1}`, the state variables are linearly interpolated:

.. math::

   x(t) = x_n + \frac{x_{n+1} - x_n}{t_{n+1} - t_n} (t - t_n)

Nozzle Mass Emission Regulation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Nozzle flow rate :math:`Q(t)` [L/min] is converted to pesticide active ingredient mass emission rate :math:`S_m(t)` [g/s] using the formulation fluid density :math:`\rho_{\text{form}}` [g/L] and active ingredient mass fraction :math:`f_{\text{active}}`:

.. math::

   S_m(t) = \frac{Q(t)}{60} \cdot \rho_{\text{form}} \cdot f_{\text{active}} \cdot a(t)

If speed-dependent rate regulation is enabled to maintain constant ground deposition density despite speed variations, the emission scales relative to the base calibration flight speed :math:`U_{\text{base}}`:

.. math::

   S_m(t) = S_{m,\text{base}}(t) \cdot \left( \frac{U(t)}{U_{\text{base}}} \right)

Dynamic Moving Source Dispersion
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Emitted pesticide mass is transported as moving sources:

1. **Gaussian Puffs**: Puffs of mass :math:`M_p = S_m(t) \cdot \Delta t` are released at the drone's instantaneous position :math:`\mathbf{x}_d(t)`. Puffs are advected with the 3D wind velocity :math:`\mathbf{u}(\mathbf{x}_p)` and grow over time with horizontal and vertical diffusivities :math:`K_h` and :math:`K_v`. Ground deposition and reflection are modeled using image sources.

2. **Lagrangian Particles (LPDM)**: Dispersed droplets are modeled as discrete particles carrying mass fractions. At each step, new particles are emitted from the nozzle and advected via:

   .. math::

     x_p(t + \Delta t) = x_p(t) + u_p \Delta t + \xi_x \sqrt{2 K_h \Delta t}

   where :math:`\xi_p \sim \mathcal{N}(0, 1)` represents turbulent stochastic diffusion.

Rotor Downwash Jet Parameterization
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To capture the local downward air jet created by the drone's rotors and its influence on droplet penetration into dense crop canopies, the module implements an analytical downwash velocity field parameterization.

The model represents the three core physical processes:

1. **Induced Velocity (Actuator Disk Theory)**:
   The induced downwash velocity at the rotor disk :math:`v_0` is calculated from the drone's mass :math:`M` and rotor radius :math:`R`:

   .. math::

      v_0 = \sqrt{\frac{M \cdot g}{2 \rho_{\text{air}} \pi R^2}}

   where :math:`\rho_{\text{air}}` is air density and :math:`g = 9.81` :math:`\text{m/s}^2`.

2. **Wake Deflection, Expansion & Decay**:
   As the jet travels downward (distance :math:`\Delta z = z_d - z \ge 0`), it expands radially and centerline velocity decays:

   .. math::

      R_j(\Delta z) = R + \alpha_{\text{jet}} \Delta z

      W_c(\Delta z) = v_0 \frac{R}{R_j(\Delta z)}

   where :math:`\alpha_{\text{jet}}` is the jet expansion coefficient. The jet centerline is deflected backward due to drone flight velocity :math:`\mathbf{V}_f = (V_x, V_y)`:

   .. math::

      x_c = x_d - V_x \frac{\Delta z}{v_0}, \quad y_c = y_d - V_y \frac{\Delta z}{v_0}

   The local downward velocity before ground effect is:

   .. math::

      w_{\text{wash\_down}}(r, \Delta z) = W_c(\Delta z) \exp\left(-\frac{r^2}{R_j(\Delta z)^2}\right)

   where :math:`r = \sqrt{(x - x_c)^2 + (y - y_c)^2}`.

3. **Ground Effect Dampening & Wall-Jet Spreading**:
   As the downward jet approaches the terrain boundary or canopy at height :math:`z_g`, the vertical velocity is dampened and redirected into a radial outward wall-jet. For height above ground :math:`h = z - z_g`:

   .. math::

      f_{\text{damp}}(h) = 1.0 - \exp\left(-\left(\frac{h}{d_{\text{damp}}}\right)^2\right)

      w_{\text{wash}} = - w_{\text{wash\_down}} \cdot f_{\text{damp}}(h)

   The vertical momentum is converted to radial horizontal velocity :math:`v_r`, creating outward spreading close to the terrain boundary:

   .. math::

      v_r(r, h) = w_{\text{wash\_down}} \cdot (1.0 - f_{\text{damp}}(h)) \cdot \frac{r}{R_j} \cdot \exp\left(-\frac{h}{d_{\text{wall\_jet}}}\right)

   which is then resolved into Cartesian components :math:`u_{\text{wash}}` and :math:`v_{\text{wash}}`.

Python API Usage
~~~~~~~~~~~~~~~~

.. code-block:: python

   from wind_solver import WindSolver
   from agricultural_drone import DroneTrajectory, MassEmissionRegulator, DronePuffDispersion

   # 1. Load flight telemetry
   trajectory = DroneTrajectory.from_csv("flight_telemetry.csv")

   # 2. Configure pesticide regulator (10% active ingredient, 1000 g/L density)
   regulator = MassEmissionRegulator(
      formulation_density=1000.0,
      active_fraction=0.1,
      base_speed=5.0,
      speed_dependent=True
   )

   # 3. Solve microclimate wind field over complex terrain
   wind = WindSolver("inputs.i")
   wind.solve()

   # 4. Initialize and execute moving source dispersion
   dispersion = DronePuffDispersion()
   dispersion.simulate(
      trajectory=trajectory,
      regulator=regulator,
      wind_solver=wind,
      dt=1.0,
      K_h=1.2,
      K_v=0.6,
      enable_ground_reflection=True,
      enable_rotor_downwash=True,   # Superimpose analytical rotor downwash velocity
      drone_mass=15.0,              # Drone mass in kg
      rotor_radius=0.4              # Rotor radius in meters
   )

   # 5. Extract 3D pesticide active ingredient concentration map [g/m³]
   concentration = dispersion.concentration  # shape: (nz, ny, nx)
   print(f"Peak concentration: {concentration.max():.4f} g/m³")

   wind.finalize()

