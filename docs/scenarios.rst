Infrastructure Vulnerability Assessment Scenarios
===================================================

This section documents realistic, physics-rich test cases demonstrating infrastructure vulnerability assessment in complex terrain scenarios.

Overview
--------

The scenarios showcase advantages over simplified tools like NOAA wind maps and NREL terrain models by:

1. **Enforcing divergence-free flow** (∇·u = 0) for physically consistent wind fields
2. **Resolving local terrain effects** explicitly (gap flow, canyon vorticity, buoyancy)
3. **Computing continuous loading profiles** along infrastructure routes
4. **Coupling thermal and mechanical effects** (heat flux → structural response)
5. **Providing real-time decision support** (comfort, safety, dynamic ratings)

Scenario 1: Altamont Pass 500 kV Transmission Line
---------------------------------------------------

Physical Context
^^^^^^^^^^^^^^^^

The Altamont Pass (CA) is one of the world's windiest locations, famous for **gap flow** wind acceleration due to pressure-driven channeling through a 5-8 km wide valley. Ambient winds of 10-12 m/s can accelerate to 25-30+ m/s in the pass core.

Why this matters:

- Transmission line thermal rating drops exponentially with wind speed
- Gap flow is pressure-driven (Bernoulli effect), not captured by point wind observations
- Dynamic ampacity ratings require continuous wind profiles along line
- Sag and tension calculations critical for clearance over terrain

Key Physics
^^^^^^^^^^^

+---------------------------+-----------+-------------------------------------+
| Aspect                    | Value     | Note                                |
+===========================+===========+=====================================+
| Domain size               | 120 km    | W-E across pass                     |
+---------------------------+-----------+-------------------------------------+
| Terrain elevation range   | 200-600 m | Gap elevation ~400 m                |
+---------------------------+-----------+-------------------------------------+
| Expected wind amplification | 1.5-3×  | Gap flow channeling                 |
+---------------------------+-----------+-------------------------------------+
| Transmission line spans   | ~300      | 350 m tower spacing                 |
+---------------------------+-----------+-------------------------------------+
| Conductor diameter        | 28 mm     | ACSR bundled                        |
+---------------------------+-----------+-------------------------------------+
| Operating current         | 500-1500 A | Typical                             |
+---------------------------+-----------+-------------------------------------+
| Critical frequency range  | 10-30 Hz  | Vortex shedding on bundles          |
+---------------------------+-----------+-------------------------------------+

Running the Scenario
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    cd tests_and_examples/altamont_pass_transmission/
    python scenario_generator.py .
    # Generates: altamont_terrain.csv, altamont_wires.csv, inputs_altamont.i

    /path/to/wind_solver inputs_altamont.i

    python verify_transmission_line.py altamont_wire_output.csv

Expected Outputs
^^^^^^^^^^^^^^^^

- **Wire loading:** Drag forces 50-200% higher in pass vs. approach
- **Thermal rating:** Dynamic ampacity 30-50% lower during high-wind periods
- **Sag assessment:** Conductor drop 0.5-2 m variation over line length
- **Resonance risk:** Vortex shedding near 10-15 Hz on bundled phases

Comparison to NOAA/NREL
^^^^^^^^^^^^^^^^^^^^^^^

+-----------------------+----------------+----------------+--------------------+
| Aspect                | NOAA 10 km grid | NREL WAsP      | massconsistent_amr |
+=======================+================+================+====================+
| Gap flow resolution   | ✗ (coarse)     | ~ (wind map)   | ✓ (flow physics)   |
+-----------------------+----------------+----------------+--------------------+
| Line-following profile| ✗ (point estim)| ~ (sparse)     | ✓ (continuous)     |
+-----------------------+----------------+----------------+--------------------+
| Dynamic ampacity      | ✗ (static)     | ✗ (static)     | ✓ (real-time)      |
+-----------------------+----------------+----------------+--------------------+
| Sag/tension calc.     | ✗              | ✗              | ✓                  |
+-----------------------+----------------+----------------+--------------------+
| Update frequency      | Daily forecast | Annual map     | Real-time obs.     |
+-----------------------+----------------+----------------+--------------------+

Scenario 2: Gorge Bridge Crossing
----------------------------------

Physical Context
^^^^^^^^^^^^^^^^

A long-span suspension/cable bridge crossing a deep gorge (300-500 m drop) with asymmetric canyon walls. Examples: Royal Gorge Bridge (CO), Millau Viaduct (FR), Foresthill Bridge (CA).

Why this matters:

- Canyon geometry creates **vertical wind shear** and cross-wind gusts
- Asymmetric walls → asymmetric loading on deck
- Low natural frequency (0.1-0.3 Hz) → resonance risk from vortex streets
- Comfort standards (ISO 6954) require acceleration estimates → dynamic analysis
- Cable tension couples to wind speed → safety-critical

Key Physics
^^^^^^^^^^^

+---------------------------+-----------+-------------------------------------+
| Aspect                    | Value     | Note                                |
+===========================+===========+=====================================+
| Canyon depth              | 300-500 m | Walls create confinement             |
+---------------------------+-----------+-------------------------------------+
| Canyon width              | 500-1000 m | Aspect ratio 0.5-1.0                |
+---------------------------+-----------+-------------------------------------+
| Wind amplification        | 50-80%    | Valley channeling + terrain         |
+---------------------------+-----------+-------------------------------------+
| Bridge main span          | 1200 m    | Cable-stayed                        |
+---------------------------+-----------+-------------------------------------+
| Deck elevation            | 900 m     | 300 m above canyon floor            |
+---------------------------+-----------+-------------------------------------+
| Natural frequency         | 0.10-0.25 Hz | Long cable spans                    |
+---------------------------+-----------+-------------------------------------+
| Expected lateral sway     | 0.5-1.5 m | Significant motion                  |
+---------------------------+-----------+-------------------------------------+
| Comfort threshold         | 0.5 m/s²  | ISO 6954                            |
+---------------------------+-----------+-------------------------------------+

Running the Scenario
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    cd tests_and_examples/gorge_bridge_crossing/
    python scenario_generator.py .
    # Generates: gorge_terrain.csv, gorge_bridge.csv, inputs_gorge.i

    /path/to/wind_solver inputs_gorge.i

    python verify_bridge_loading.py gorge_bridge_output.csv

Expected Outputs
^^^^^^^^^^^^^^^^

- **Sway angle:** 0.5-2.0 degrees (main span)
- **Bending moment:** 2-5 MN·m (varies with vertical shear)
- **Vertical acceleration:** 0.1-0.3 m/s² (near comfort limit)
- **Resonance ratio:** 0.8-1.2 (vortex shedding near natural frequency)
- **Comfort assessment:** 0.4-0.8 (unsafe for pedestrians in gusts)

Comparison to NOAA/NREL
^^^^^^^^^^^^^^^^^^^^^^^

+---------------------+------------------+------------------+--------------------+
| Aspect              | Standard design  | NOAA estimate    | massconsistent_amr |
+=====================+==================+==================+====================+
| Wind profile res.   | 3 points         | 1-2 points       | 100+ cells/span    |
+---------------------+------------------+------------------+--------------------+
| Vertical shear      | Assumed z^n      | Constant         | Computed (physics) |
+---------------------+------------------+------------------+--------------------+
| Vortex resonance    | Design factor ×1.4 | Not addressed  | Dynamic prediction |
+---------------------+------------------+------------------+--------------------+
| Comfort metric      | Not assessed     | —                | Real-time accel    |
+---------------------+------------------+------------------+--------------------+
| Dynamic sway        | Design standard  | Not estimated    | Time-series        |
+---------------------+------------------+------------------+--------------------+
| Gust modeling       | Discrete gusts   | Single estimate  | Continuous field   |
+---------------------+------------------+------------------+--------------------+

Scenario 3: Urban Heat Island Building
---------------------------------------

Physical Context
^^^^^^^^^^^^^^^^

A 200 m tall commercial tower in a dense urban block (Manhattan/London style) where **street canyon geometry** and **thermal buoyancy** from urban heat island modify wind loading.

Why this matters:

- Street canyon winds can be 2-4× ambient (funnel effect)
- Urban heat island (ΔT = +3-8°C) creates buoyancy → reduces vertical wind
- Building clustering creates pressure-driven flow at street level
- Thermal effects change stability class → modify turbulence
- Real-time wind-induced sway affects occupant comfort and operations

Key Physics
^^^^^^^^^^^

+---------------------------+-----------+-------------------------------------+
| Aspect                    | Value     | Note                                |
+===========================+===========+=====================================+
| Domain size               | 5 km × 5 km | Urban block cluster                |
+---------------------------+-----------+-------------------------------------+
| Building heights          | 100-200 m | 50-story typical                    |
+---------------------------+-----------+-------------------------------------+
| Street canyon width       | 30-40 m   | ~8-10 story aspect ratio            |
+---------------------------+-----------+-------------------------------------+
| Urban roughness z₀        | 1-2 m     | vs. 0.1 m suburban                  |
+---------------------------+-----------+-------------------------------------+
| Heat island ΔT            | +3°C      | Surface flux ~200 W/m²              |
+---------------------------+-----------+-------------------------------------+
| Wind amplification        | 40-100%   | Street canyon + canopy              |
+---------------------------+-----------+-------------------------------------+
| Tower natural freq.       | 0.25 Hz   | 200 m commercial                    |
+---------------------------+-----------+-------------------------------------+
| Peak acceleration         | 0.1-0.3 m/s² | Comfort threshold ~0.2 m/s²        |
+---------------------------+-----------+-------------------------------------+
| Expected lateral sway     | 0.3-0.8 m | 200 m height                        |
+---------------------------+-----------+-------------------------------------+

Running the Scenario
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    cd tests_and_examples/urban_heat_island_building/
    python scenario_generator.py .
    # Generates: urban_terrain.csv, urban_building.csv, inputs_urban.i

    /path/to/wind_solver inputs_urban.i

    python verify_structure_loading.py urban_building_output.csv

Expected Outputs
^^^^^^^^^^^^^^^^

- **Base shear force:** 5-15 MN (varies with street canyon flow)
- **Overturning moment:** 500-1500 MN·m
- **Lateral deflection:** 0.3-0.8 m at building top
- **Stress ratio:** 0.05-0.15 (well below yield)
- **Acceleration:** 0.1-0.3 m/s² (comfort metric)
- **Damage state:** NONE (robust modern design)

Comparison to NOAA/NREL
^^^^^^^^^^^^^^^^^^^^^^^

+---------------------+------------------+------------------+--------------------+
| Aspect              | Simplified estim | WAsP-style       | massconsistent_amr |
+=====================+==================+==================+====================+
| Canopy roughness    | z₀ = 0.5-1.0 m  | Grid-averaged    | Local cell values  |
+---------------------+------------------+------------------+--------------------+
| Thermal effects     | Not included     | Not included     | Coupled buoyancy   |
+---------------------+------------------+------------------+--------------------+
| Street canyon       | Ignored          | Ignored          | Explicit geometry  |
+---------------------+------------------+------------------+--------------------+
| Building interaction | Single building | Isolated building | Full clustering    |
+---------------------+------------------+------------------+--------------------+
| Comfort prediction  | Design standard  | Not addressed    | Real-time accel    |
+---------------------+------------------+------------------+--------------------+
| Update frequency    | Static design    | Annual maps      | Real-time obs.     |
+---------------------+------------------+------------------+--------------------+
| Multi-physics       | Wind only        | Wind only        | Wind + thermal     |
+---------------------+------------------+------------------+--------------------+

General Advantages of massconsistent_amr
----------------------------------------

Physics Fidelity
^^^^^^^^^^^^^^^^

- **Continuity equation** enforced locally → no unphysical divergence
- **Pressure gradient** computed from terrain/thermal forcing
- **Vertical velocity** properly accounted (w-component)
- **Buoyancy coupling** for heat island and slope flows
- **Turbulence parameterization** adapted to local stability

Spatial Resolution
^^^^^^^^^^^^^^^^^^

+--------------------+----------+----------+----------+
| Tool               | Horizontal | Vertical | Time     |
+====================+============+==========+==========+
| NOAA GFS           | 13 km    | 8-15 levels | 6 hr     |
+--------------------+----------+----------+----------+
| NOAA HRRR          | 3 km     | ~50 levels | 1 hr     |
+--------------------+----------+----------+----------+
| NREL WAsP          | 50-500 m | 3-5 points | Annual   |
+--------------------+----------+----------+----------+
| **massconsistent_amr** | **10-100 m** | **10-100 m** | **Real-time** |
+--------------------+----------+----------+----------+

Infrastructure-Specific Metrics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Transmission line:** Dynamic ampacity, sag, tension
- **Bridge:** Lateral sway, bending moment, comfort, resonance
- **Building:** Base shear, overturning moment, deflection, acceleration
- **Wind farm:** Individual turbine wind speed, power, wake effects

Operational Benefits
^^^^^^^^^^^^^^^^^^^^^

✓ Real-time risk assessment (no 6-12 hr forecast lag)
✓ Spatially continuous profiles (not point estimates)
✓ Thermally coupled (heat → buoyancy → sway reduction)
✓ Dynamic ratings (vs. conservative static tables)
✓ Integrated terrain/building effects (vs. additive)
✓ Open-source & customizable (vs. proprietary)

Running All Scenarios
---------------------

.. code-block:: bash

    #!/bin/bash
    SOLVER="/path/to/wind_solver"

    for scenario in altamont_pass_transmission gorge_bridge_crossing urban_heat_island_building; do
        cd tests_and_examples/$scenario
        python scenario_generator.py .
        $SOLVER inputs_*.i
        python verify_*.py *_output.csv
        cd ../../..
    done

References & Further Reading
------------------------------

Key literature supporting these scenarios:

**Altamont Pass Transmission Scenario**

- Delparte, C., Hacker, J. P., & Jiménez, M. (2000). Gap flow wind acceleration in the Altamont Pass, California. *Journal of Applied Meteorology*, 39(5), 619–635.
- IEEE 738 (2012). *Standard for calculating current-temperature relationship of bare overhead conductors*. IEEE Power & Energy Society.
- Mathiesen, A. M., & Svitra, P. (2003). *Dynamic thermal line rating system for composite overhead transmission lines*. CIGRE Technical Brochure 207.

**Gorge Bridge Crossing Scenario**

- Simiu, E., & Scanlan, R. H. (1996). *Wind Effects on Structures: Fundamentals and Applications to Design* (3rd ed.). Wiley-Interscience.
- Yamaguchi, H. (1992). Analytical and experimental studies on aerodynamic instabilities of cable-stayed bridges. *Journal of Wind Engineering and Industrial Aerodynamics*, 33(3–4), 371–389.
- ISO 6954:2010. *Mechanical vibration — Guidelines for the measurement and evaluation of vibration and its effects on buildings*. International Organization for Standardization.

**Urban Heat Island Building Scenario**

- Oke, T. R. (1987). *Boundary Layer Climates* (2nd ed.). Methuen.
- Oke, T. R. (1988). Street design and urban canopy layer climate. *Energy and Buildings*, 11(3), 103–113.
- Yokoyama, H., Oikawa, S., & Miyashita, K. (2010). Large-eddy simulation of thermal effects on wind characteristics over an urban canopy. *Journal of Wind Engineering and Industrial Aerodynamics*, 98(8–9), 405–413.

Complete reference database with additional citations for wind engineering, fluid mechanics, and computational methods is available in :ref:`the complete reference section <references>`.
