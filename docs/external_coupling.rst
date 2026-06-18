.. _external_coupling:

External Coupling
=================

This section describes the interfaces, python couplings, and mathematical frameworks for coupling the Mass-Consistent AMR Wind Solver with external physics solvers and optimization tools.

Wind Farm Interoperability (Phase 1)
------------------------------------

**Overview**

Phase 1 features provide file format compatibility and data exchange with Floris and PyOptimization wind farm optimization tools. Three key utilities enable standardized workflows:

1. **CSV Turbine Definition Format** — Read/write turbine layouts as comma-separated values
2. **Wind Resource Summary Statistics** — Compute statistics (mean, std, Weibull parameters) from wind fields
3. **PyOptimization Result Export** — Export farm simulation results in Floris-compatible JSON and CSV formats

**Location**

Phase 1 implementations are located in: ``src/python/``

The corresponding demonstration and tests are in: ``tests_and_examples/phase1_features/``

**Modules**

1. ``turbine_io.py`` — Turbine layout I/O (Feature 1)
2. ``wind_resource_stats.py`` — Wind statistics computation (Feature 2)
3. ``pyoptimization_export.py`` — PyOptimization export (Feature 3)
4. ``test_phase1_features.py`` — Comprehensive unit tests (18 test cases)

**Example Usage**

See ``tests_and_examples/phase1_features/test_phase1_wind_farm.py`` for a complete demonstration of all three Phase 1 features.

Run demonstration:

.. code-block:: bash

    cd tests_and_examples/phase1_features
    python3 test_phase1_wind_farm.py

Run unit tests:

.. code-block:: bash

    cd src/python
    python3 test_phase1_features.py -v

**Feature 1: CSV Turbine Definition Format**

The ``TurbineLayout`` class provides read/write capabilities for turbine layouts.

CSV Format:

.. code-block:: text

    turbine_id, x_m, y_m, z_agl_m, turbine_type, hub_height, rotor_diameter, power_curve_file
    0, 100.0, 200.0, 0.0, DTU10MW, 90.0, 178.0, power_curves/dtu10mw.json
    1, 500.0, 200.0, 50.0, NREL15MW, 120.0, 240.0, power_curves/nrel15mw.json

Python interface:

.. code-block:: python

    from turbine_io import TurbineLayout
    
    # Load layout from CSV
    layout = TurbineLayout.read_csv("turbines.csv")
    
    # Validate spacing (minimum 400m between turbines)
    is_valid, errors = layout.validate_spacing(min_spacing=400.0)
    
    # Export to CSV
    TurbineLayout.write_csv(layout, "output_turbines.csv")

**Feature 2: Wind Resource Summary Statistics**

The ``WindResourceStats`` class computes statistical summaries of wind fields.

Computed metrics:

- Mean wind speed and direction
- Standard deviation (speed and direction)
- Wind speed range (min/max)
- Weibull distribution parameters (shape k, scale c)
- Turbulence intensity indicators
- Wind rose statistics

Python interface:

.. code-block:: python

    from wind_resource_stats import WindResourceStats
    import numpy as np
    
    # Compute statistics from 2D wind field at hub height
    u_field = wind_solver.get_velocity_at_agl(90.0)['u']
    v_field = wind_solver.get_velocity_at_agl(90.0)['v']
    
    stats = WindResourceStats.compute_from_wind_field(
        u_field, v_field, height_agl=90.0
    )
    
    # Access computed values
    print(f"Mean speed: {stats.mean_speed:.2f} m/s")
    print(f"Weibull k: {stats.weibull_k:.2f}")
    
    # Export to JSON
    stats.to_json("wind_stats.json")
    
    # Display summary
    print(stats.summary_string())

**Feature 3: PyOptimization Result Export**

The ``PyOptimizationExporter`` class exports farm results in Floris-compatible formats.

Supported output formats:

- JSON (PyOptimization-compatible schema)
- CSV (per-turbine results)
- CSV (farm-level summary)

JSON output structure:

.. code-block:: json

    {
      "metadata": {
        "farm_name": "Example_Farm",
        "version": "1.0"
      },
      "farm_summary": {
        "num_turbines": 4,
        "total_power_kw": 16200.0,
        "annual_energy_gwh": 141.9
      },
      "wind_resource": {
        "mean_speed_ms": 10.25,
        "mean_direction_deg": 270.0,
        "turbulence_intensity": 0.08
      },
      "turbines": [
        {
          "id": 0,
          "location": {"x_m": 100.0, "y_m": 200.0},
          "power": {"output_kw": 4000.0, "thrust_coefficient": 0.82},
          ...
        }
      ]
    }

Python interface:

.. code-block:: python

    from pyoptimization_export import PyOptimizationExporter
    
    # Create exporter
    exporter = PyOptimizationExporter("My_Wind_Farm")
    
    # Add per-turbine results
    exporter.add_turbine_result(
        turbine_id=0, x=100.0, y=200.0,
        power_kw=4000.0, wind_speed_ms=10.0, wind_direction_deg=270.0,
        thrust_coefficient=0.82, hub_height=90.0, rotor_diameter=100.0
    )
    
    # Set farm-level aggregates
    exporter.set_farm_power(total_power_kw=16200.0, annual_energy_gwh=141.9)
    
    # Set wind resource statistics
    exporter.set_wind_resource(
        mean_speed_ms=10.25, mean_direction_deg=270.0,
        turbulence_intensity=0.08
    )
    
    # Export to formats
    exporter.export_json("results.json")
    exporter.export_turbine_csv("turbine_results.csv")
    exporter.export_summary_csv("farm_summary.csv")

PHREEQC Coupling
----------------

Overview
~~~~~~~~

The PHREEQC coupling framework provides one-way integration with geochemical reactive transport solvers for wind-driven studies, such as critical mineral leaching, acid mine drainage (AMD) analysis, and contaminant transport with terrain-resolved atmospheric boundary conditions.

The coupling code is located in: ``tests_and_examples/phreeqc_coupling/``

The directory contains 11 standalone example scripts demonstrating core capabilities:

1. **wind_field_bc.py** — Wind velocity as boundary condition for pore-water advection
2. **temperature_profile_bc.py** — Temperature profile extraction from wind solver
3. **precipitation_recharge.py** — Infiltration mapping and recharge calculations
4. **kv_dispersivity.py** — Vertical permeability and dispersivity extraction
5. **stability_classification.py** — Pasquill-Gifford-Turner stability classification
6. **valley_amd_hotspots.py** — Acid mine drainage hotspot detection in valleys
7. **sulfide_oxidation.py** — Oxidation kinetics for sulfide minerals
8. **spatial_temperature_cache.py** — Scenario caching for rapid deployments
9. **dust_suppression.py** — Dust settling and suppression calculations
10. **leaching_efficiency_sherwood.py** — Leaching enhancement via Sherwood number
11. **end_to_end_facility.py** — Complete workflow demonstration

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


Phase 4: Canopy Interaction & Leaf/Ground Deposition Mapping
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To model dynamic pesticide deposition and capture by plant foliage and soil, the module supports specialized crop canopy interactions:

1. **Foliage Interception & Droplet Size Efficiency**:
   Capture of settling droplets by crop foliage is resolved into vertical settling capture and horizontal wind drift capture. The collection efficiency :math:`\eta_d` depends on droplet size:

   .. math::

      \eta_d = 1.0 - \exp\left(-\frac{d}{d_{\text{ref}}}\right)

   where :math:`d` is the local droplet diameter and :math:`d_{\text{ref}} = 100 \times 10^{-6}` :math:`\text{m}` is the reference droplet size.
   
   The vertical interception rate constant :math:`k_{\text{dep, vert}}` (representing capture of settling/falling droplets by horizontal leaf surfaces) and the horizontal interception rate constant :math:`k_{\text{dep, horiz}}` (representing capture of horizontally drifting droplets due to wind) are modeled as:

   .. math::

      k_{\text{dep, vert}} = \frac{v_s}{H_c} \cdot \text{LAI} \cdot \eta_d \cdot \lambda_{\text{vert}}

      k_{\text{dep, horiz}} = \frac{U_h}{H_c} \cdot \text{FAI} \cdot \eta_d \cdot \lambda_{\text{horiz}}

   where :math:`H_c` is the local canopy height, :math:`\text{LAI}` is the Leaf Area Index, :math:`\text{FAI}` is the crop Frontal Area Index, :math:`v_s` is the terminal settling velocity, :math:`U_h = \sqrt{u^2 + v^2}` is the horizontal wind speed, and :math:`\lambda_{\text{vert}} = \lambda_{\text{horiz}} = 0.5` are empirical interception coefficients.
   
   The combined foliage capture rate inside the canopy (:math:`0 \le z_{\text{agl}} < H_c`) is:

   .. math::

      k_{\text{foliage}} = k_{\text{dep, vert}} + k_{\text{dep, horiz}}

   The intercepted mass over a timestep :math:`\Delta t` is subtracted from the particle/puff mass and mapped to 2D deposition grids:

   .. math::

      \Delta M = M \cdot \left(1.0 - \exp(-k_{\text{foliage}} \Delta t)\right)

2. **Spatially Distributed 2D Deposition Grids**:
   The module maintains cell-local cumulative registers of deposited pesticide mass [g] across three compartments:
   - **Canopy Top**: Upper canopy foliage layers (:math:`0.5 H_c \le z_{\text{agl}} < H_c`).
   - **Lower Foliage Layers**: Lower foliage layers (:math:`0 \le z_{\text{agl}} < 0.5 H_c`).
   - **Underlying Ground**: Soil/ground level deposition (:math:`z_{\text{agl}} \le 0` or upon ground impact).

3. **Mass Conservation Verification**:
   The solver validates pesticide mass conservation at every step by checking that the total emitted mass from the nozzle balances exactly with all active and lost compartments:

   .. math::

      M_{\text{emitted}} = M_{\text{airborne}} + M_{\text{canopy\_top}} + M_{\text{lower\_foliage}} + M_{\text{ground}} + M_{\text{out\_of\_bounds}} + M_{\text{degraded}}

   You can verify this mass balance programmatically via:

   .. code-block:: python

      conserved, balance = dispersion.verify_mass_conservation()
      if conserved:
          print("Pesticide mass is fully conserved!")
          print(f"Total Emitted: {balance['total_emitted_mass']} g")
          print(f"Total Accounted: {balance['total_accounted']} g")

Simple Reactive Chemistry (AERMOD TOXICS Level)
-----------------------------------------------

Overview
~~~~~~~~

The simple reactive chemistry module enables first-order exponential decay of chemical species in plume transport, matching EPA AERMOD TOXICS capabilities. This module supports passive tracer decay with stoichiometric product formation, optional seasonal variation, and temperature-dependent reaction rates.

Supported Species
~~~~~~~~~~~~~~~~~

**Reactants:**

- **NO₂ (Nitrogen Dioxide)**: Photochemical decay to NO with default 4-hour half-life
- **SO₂ (Sulfur Dioxide)**: Oxidation to SO₄²⁻ with default 24-hour half-life
- **HCl (Hydrogen Chloride)**: Hydrolysis to Cl⁻ with default 12-hour half-life
- **Custom species**: User-specified 1st-order decay constants

**Products:**

- **NO (Nitric Oxide)**: Produced from NO₂ decay (1:1 molar stoichiometry)
- **SO₄²⁻ (Sulfate Ions)**: Produced from SO₂ oxidation (1.5:1 mass ratio)
- **Cl⁻ (Chloride Ions)**: Produced from HCl hydrolysis (0.97:1 mass ratio)

Physics Basis
~~~~~~~~~~~~~

**First-Order Exponential Decay:**

.. math::

   C(t) = C(0) \times \exp(-\lambda t)

where :math:`\lambda = \frac{\ln(2)}{t_{1/2}}` and :math:`t_{1/2}` is the half-life.

**Product Formation (Mass Conservation):**

.. math::

   C_{\text{product}}(t) = C_{\text{product}}(0) + 
   [C_{\text{reactant}}(0) - C_{\text{reactant}}(t)] \times r_s

where :math:`r_s` is the stoichiometric ratio (mass basis).

**Temperature Correction (Q10 Model):**

.. math::

   \lambda(T) = \lambda(T_{\text{ref}}) \times Q_{10}^{\frac{T - T_{\text{ref}}}{10}}

Default :math:`Q_{10} = 2.0` (doubling per 10 K).

**Seasonal Adjustment:**

Empirical factors applied to decay constants:
- **Winter (Dec-Feb)**: 0.5× - 0.7× (reduced oxidation, less UV)
- **Summer (Jun-Aug)**: 1.3× - 1.5× (enhanced oxidation, more UV)
- **Spring/Fall**: 1.0× (intermediate)

Configuration
~~~~~~~~~~~~~

In the input file (``inputs.i``):

.. code-block:: bash

   # Enable simple reactive chemistry
   puff_chemistry_enabled = true
   
   # Half-lives [hours]
   puff_chemistry_half_life_NO2 = 4.0    # NO₂ photolysis
   puff_chemistry_half_life_SO2 = 24.0   # SO₂ oxidation
   puff_chemistry_half_life_HCL = 12.0   # HCl hydrolysis
   
   # Track decay products
   puff_chemistry_enable_products = true
   
   # Optional: Enable seasonal variation
   puff_chemistry_enable_seasonal_adjust = false
   
   # Optional: Enable temperature correction
   puff_chemistry_enable_temp_adjust = false
   puff_chemistry_temp_ref = 298.15      # Reference temperature [K]
   puff_chemistry_Q10 = 2.0               # Temperature sensitivity
   
   # Month for seasonal adjustment (1-12)
   puff_chemistry_month = 6
   
   # Initial species concentrations [ppb or μg/m³]
   puff_initial_NO2 = 100.0
   puff_initial_SO2 = 200.0
   puff_initial_HCL = 50.0
   puff_initial_NO = 0.0
   puff_initial_SO4 = 0.0
   puff_initial_CL = 0.0

Usage Examples
~~~~~~~~~~~~~~

**Example 1: NO₂ Decay**

.. code-block:: bash

   puff_chemistry_enabled = true
   puff_chemistry_half_life_NO2 = 4.0  # 4-hour half-life
   puff_chemistry_enable_products = true
   puff_initial_NO2 = 100.0            # 100 ppb initial

After 4 hours of transport (at 10 m/s wind: ~144 km downwind):
- C(NO₂) ≈ 50 ppb (50% remaining)
- C(NO) ≈ 50 ppb (produced from decay)

**Example 2: SO₂ with Seasonal Adjustment**

.. code-block:: bash

   puff_chemistry_enabled = true
   puff_chemistry_half_life_SO2 = 24.0
   puff_chemistry_enable_seasonal_adjust = true
   puff_chemistry_month = 7             # July (summer)
   puff_initial_SO2 = 200.0

Summer (month=7):
- Effective half-life: ~16 hours (24 / 1.5)
- Faster oxidation rate

Winter (month=1):
- Effective half-life: ~48 hours (24 / 0.5)
- Slower oxidation rate

Output Fields
~~~~~~~~~~~~~

When chemistry is enabled, the following concentration fields are available in output files:

- ``C_NO2`` — Nitrogen dioxide concentration [ppb]
- ``C_SO2`` — Sulfur dioxide concentration [ppb]
- ``C_HCL`` — Hydrogen chloride concentration [ppb]
- ``C_NO`` — Nitric oxide concentration [ppb]
- ``C_SO4`` — Sulfate ion concentration [ppb]
- ``C_CL`` — Chloride ion concentration [ppb]

Regression Tests
~~~~~~~~~~~~~~~~

Two reference test cases validate chemistry functionality:

1. **puff_chemistry_no2_decay**: 4-hour half-life decay of 100 ppb NO₂ over 4-hour transport time. Expected: ~50 ppb remaining.

2. **puff_chemistry_so2_oxidation**: 24-hour half-life oxidation of 200 ppb SO₂ over 5.56-hour transport. Expected: ~142 ppb SO₂, ~105 ppb SO₄.

Run tests with:

.. code-block:: bash

   cd build
   ctest -R "puff_chemistry" -V

Limitations and Future Work
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Current Limitations:**

- No multi-species feedback (each species decays independently)
- No photolysis angle dependence (uses fixed half-lives)
- No humidity or cloud effects on oxidation rates
- No radical chemistry (OH, HO₂, NO₃)
- Product formation is stoichiometric only (no intermediate products)

**Future Enhancements:**

- NOₓ-O₃ photochemical cycle with feedback
- Humidity-dependent SO₂ oxidation
- Photolysis rates based on solar angle and cloud cover
- Radical chemistry mechanisms (simplified CAMx-level)
- Aqueous-phase chemistry (clouds/fog)
- Temperature/humidity-dependent rate constants from literature

References
~~~~~~~~~~

- EPA (2005). AERMOD TOXICS Module: Reactive Tracer Formulation
- Finlayson-Pitts, B. J., & Pitts, J. N. (2000). Chemistry of the Upper and Lower Atmosphere. Academic Press.
- Atkinson, R., Baulch, D. L., Cox, R. A., et al. (2004). Evaluated kinetic and photochemical data for atmospheric chemistry. Atmos. Chem. Phys., 4, 1461-1738.


