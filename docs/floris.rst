.. _floris:

FLORIS Integration
==================

Overview
--------

The mass-consistent AMR wind solver can export wind field data in formats compatible with 
`FLORIS <https://nrel.github.io/floris/>`_, the National Renewable Energy Laboratory's 
wind farm simulation software. This enables seamless integration of high-fidelity terrain-aware 
wind fields with wake modeling and wind farm optimization.

The FLORIS export functionality operates **completely independently** — no FLORIS installation 
is required to generate the export data. You can use the exported data with FLORIS whenever needed.

Key Features
~~~~~~~~~~~~

- **Standalone export** — No FLORIS dependency; works with the mass-consistent solver alone
- **Multiple formats** — CSV and JSON output options
- **Flexible interpolation** — Extract wind at arbitrary turbine locations
- **Speed-up ratios** — Compute local wind enhancement relative to reference speed
- **Terrain-aware** — Respects terrain elevation and complex topography
- **Hub-height sampling** — Query wind at any height above ground level (AGL)

Export Tools
------------

Two export methods are provided:

1. **floris_export.py** — Command-line tool for exporting wind data
2. **floris_coupling.py** — Python module for programmatic access

Command-Line Tool: floris_export.py
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``floris_export.py`` tool is a standalone command-line utility that exports wind speeds 
from the mass-consistent solver to FLORIS-compatible format.

Basic Usage
^^^^^^^^^^^

Export wind at specific turbine locations to CSV::

    python3 floris_export.py \
        --solver inputs.i \
        --turbines turbines.csv \
        --hub-height 90.0 \
        --output wind_data.csv

Export with speed-up ratios relative to reference wind::

    python3 floris_export.py \
        --solver inputs.i \
        --turbines turbines.csv \
        --hub-height 90.0 \
        --reference-speed 10.0 \
        --output wind_data.csv

Export to JSON format::

    python3 floris_export.py \
        --solver inputs.i \
        --turbines turbines.csv \
        --hub-height 90.0 \
        --output wind_data.json

Command-Line Options
^^^^^^^^^^^^^^^^^^^^^

.. option:: --solver INPUTS_FILE

    Path to the mass-consistent solver inputs file (e.g., ``inputs.i``).
    Required.

.. option:: --turbines CSV_FILE

    CSV file with turbine locations. Format: two columns named ``x`` and ``y`` with 
    coordinate values in meters. Required.
    
    Example ``turbines.csv``::
    
        x,y
        100.0,200.0
        300.0,400.0
        500.0,600.0

.. option:: --hub-height HEIGHT

    Hub height above ground level (AGL) in meters. Default: 90.0 m.

.. option:: --reference-speed SPEED

    Reference wind speed in m/s for computing speed-up ratios. If not provided, 
    speed-up ratios are not included in output. Optional.

.. option:: --output FILE

    Output file path. Format is auto-detected from file extension 
    (``.csv`` or ``.json``). Required.

.. option:: --format {auto,csv,json}

    Explicitly specify output format. Default: auto-detect from output filename.

.. option:: --verbose

    Enable verbose output for debugging.

.. option:: -h, --help

    Show help message.

Output Formats
^^^^^^^^^^^^^^

CSV Output
++++++++++

Standard turbine wind data CSV format::

    turbine_id,x,y,z_terrain,z_hub,u_ms,v_ms,speed_ms,direction_deg
    0,100.0,200.0,50.0,140.0,5.2,1.3,5.33,345.2
    1,300.0,400.0,75.0,165.0,6.1,0.8,6.15,352.4
    2,500.0,600.0,45.0,135.0,4.9,1.5,5.12,341.8

With optional speed-up ratios::

    turbine_id,x,y,z_terrain,z_hub,u_ms,v_ms,speed_ms,direction_deg,speedup_ratio
    0,100.0,200.0,50.0,140.0,5.2,1.3,5.33,345.2,1.05
    1,300.0,400.0,75.0,165.0,6.1,0.8,6.15,352.4,1.12
    2,500.0,600.0,45.0,135.0,4.9,1.5,5.12,341.8,1.03

JSON Output
+++++++++++

JSON format for programmatic use::

    {
      "metadata": {
        "solver": "inputs.i",
        "hub_height_agl": 90.0,
        "reference_speed": 10.0,
        "num_turbines": 3
      },
      "turbines": [
        {
          "turbine_id": 0,
          "x": 100.0,
          "y": 200.0,
          "z_terrain": 50.0,
          "z_hub": 140.0,
          "u_ms": 5.2,
          "v_ms": 1.3,
          "speed_ms": 5.33,
          "direction_deg": 345.2,
          "speedup_ratio": 1.05
        },
        ...
      ]
    }

Python Module: floris_coupling.py
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For programmatic access, use the ``floris_coupling.py`` module.

Module Setup
^^^^^^^^^^^^

First, ensure the wind solver Python bindings are built and PYTHONPATH is set::

    cd massconsistent_amr
    cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
    cmake --build build
    export PYTHONPATH=$PWD/build/python:$PYTHONPATH

Basic Usage
^^^^^^^^^^^

Import and use the ``FLORISWindMap`` class::

    from wind_solver import WindSolver
    from floris_coupling import FLORISWindMap

    # Solve wind field
    wind = WindSolver("inputs.i")
    wind.solve()

    # Create wind map for FLORIS
    wind_map = FLORISWindMap(wind)

    # Export wind at turbine locations
    turbine_locs = [(100, 200), (300, 400), (500, 400)]
    wind_map.export_to_csv(
        turbine_locations=turbine_locs,
        hub_height=90.0,
        output_file="wind_data.csv"
    )

    wind.finalize()

FLORISWindMap Class Reference
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. class:: FLORISWindMap(wind_solver)

    High-level interface for extracting and exporting wind data in FLORIS format.

    :param wind_solver: Initialized and solved WindSolver instance
    :type wind_solver: WindSolver

    .. method:: export_to_csv(turbine_locations, hub_height, output_file, reference_speed=None)

        Export wind data to CSV format.

        :param turbine_locations: List of (x, y) tuples for turbine positions
        :type turbine_locations: List[Tuple[float, float]]
        :param hub_height: Hub height above ground level (AGL) in meters
        :type hub_height: float
        :param output_file: Path to output CSV file
        :type output_file: str
        :param reference_speed: Optional reference wind speed for speed-up ratios (m/s)
        :type reference_speed: float or None

    .. method:: export_to_json(turbine_locations, hub_height, output_file, reference_speed=None)

        Export wind data to JSON format.

        :param turbine_locations: List of (x, y) tuples for turbine positions
        :type turbine_locations: List[Tuple[float, float]]
        :param hub_height: Hub height above ground level (AGL) in meters
        :type hub_height: float
        :param output_file: Path to output JSON file
        :type output_file: str
        :param reference_speed: Optional reference wind speed for speed-up ratios (m/s)
        :type reference_speed: float or None

    .. method:: get_wind_at_location(x, y, z_agl)

        Get interpolated wind speed at a specific location.

        :param x: X coordinate (meters)
        :type x: float
        :param y: Y coordinate (meters)
        :type y: float
        :param z_agl: Height above ground level (meters)
        :type z_agl: float
        :return: Dict with keys 'u', 'v', 'w', 'speed', 'direction'
        :rtype: dict

    .. method:: export_turbine_winds(turbine_locations, hub_height, output_file, reference_speed=None)

        Convenience method to extract and export wind at turbine hubs.

        :param turbine_locations: List of (x, y) tuples
        :type turbine_locations: List[Tuple[float, float]]
        :param hub_height: Hub height AGL (meters)
        :type hub_height: float
        :param output_file: Output CSV file path
        :type output_file: str
        :param reference_speed: Optional reference speed (m/s)
        :type reference_speed: float or None

Practical Examples
------------------

Example 1: Simple Wind Farm Export
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a simple wind farm with 3 turbines and export wind data::

    from wind_solver import WindSolver
    from floris_coupling import FLORISWindMap
    import csv

    # Initialize and solve
    wind = WindSolver("inputs.i")
    wind.solve()

    # Define turbine locations (in domain coordinates)
    turbines = [
        (500, 500),    # Turbine 1
        (800, 500),    # Turbine 2
        (1100, 500),   # Turbine 3
    ]

    # Export to CSV
    wind_map = FLORISWindMap(wind)
    wind_map.export_to_csv(
        turbine_locations=turbines,
        hub_height=90.0,
        output_file="farm_wind.csv",
        reference_speed=10.0
    )

    wind.finalize()

Example 2: Variable Hub Heights
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Extract wind at different heights for multi-hub wind farms::

    from wind_solver import WindSolver
    from floris_coupling import FLORISWindMap

    wind = WindSolver("inputs.i")
    wind.solve()
    wind_map = FLORISWindMap(wind)

    # Define turbines with different hub heights
    turbines_90m = [(500, 500), (800, 500)]
    turbines_120m = [(1100, 500)]

    # Export at different heights
    wind_map.export_to_csv(turbines_90m, hub_height=90.0, 
                          output_file="farm_90m.csv")
    wind_map.export_to_csv(turbines_120m, hub_height=120.0, 
                          output_file="farm_120m.csv")

    wind.finalize()

Example 3: Multi-Scenario Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Compare wind speeds across different reference conditions::

    from wind_solver import WindSolver
    from floris_coupling import FLORISWindMap

    scenarios = [
        {"name": "scenario_1", "U_ref": 8.0, "V_ref": 0.0},
        {"name": "scenario_2", "U_ref": 10.0, "V_ref": 2.0},
        {"name": "scenario_3", "U_ref": 12.0, "V_ref": 0.0},
    ]

    turbines = [(500, 500), (800, 500), (1100, 500)]

    for scenario in scenarios:
        wind = WindSolver("inputs.i")
        wind.update_reference_wind(scenario["U_ref"], scenario["V_ref"])
        wind.solve()

        wind_map = FLORISWindMap(wind)
        wind_map.export_to_csv(
            turbine_locations=turbines,
            hub_height=90.0,
            output_file=f"{scenario['name']}_wind.csv",
            reference_speed=scenario["U_ref"]
        )
        wind.finalize()

Example 4: Reading Turbine Locations from File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Load turbine coordinates from a CSV and export wind data::

    from wind_solver import WindSolver
    from floris_coupling import FLORISWindMap
    import csv

    # Read turbine locations
    turbines = []
    with open("turbines.csv") as f:
        reader = csv.DictReader(f)
        for row in reader:
            x = float(row["x"])
            y = float(row["y"])
            turbines.append((x, y))

    # Solve and export
    wind = WindSolver("inputs.i")
    wind.solve()

    wind_map = FLORISWindMap(wind)
    wind_map.export_to_csv(
        turbine_locations=turbines,
        hub_height=90.0,
        output_file="wind_farm_winds.csv"
    )

    wind.finalize()

Integration with FLORIS Workflow
---------------------------------

Once you have exported wind data from the mass-consistent solver, you can use it 
in FLORIS workflows:

1. **Export wind data** from mass-consistent solver::

    python3 floris_export.py --solver inputs.i --turbines turbines.csv \
        --hub-height 90.0 --output wind_data.csv

2. **Parse exported data** in FLORIS::

    import pandas as pd
    import floris
    
    # Load wind data
    wind_df = pd.read_csv("wind_data.csv")
    
    # Initialize FLORIS
    fi = floris.Floris("configuration.yaml")
    
    # Set wind speeds from exported data
    for idx, row in wind_df.iterrows():
        fi.farm.wind_field.wind_speed[idx] = row["speed_ms"]
        fi.farm.wind_field.wind_direction[idx] = row["direction_deg"]
    
    # Run FLORIS simulations
    fi.farm.flow_field.calculate_wake_turbulence_velocities()
    fi.farm.calculate_power()

3. **Analyze wind farm performance**::

    power = fi.farm.turbine_power
    print(f"Total farm power: {power.sum() / 1e6:.2f} MW")
    print(f"Average turbine power: {power.mean() / 1e3:.2f} kW")

Performance Considerations
--------------------------

- **Interpolation cost** — Wind extraction at arbitrary locations involves spatial 
  interpolation; multiple queries at the same location should be cached
- **Export latency** — CSV/JSON export is negligible compared to wind solve time
- **Memory usage** — Wind field data is kept in memory during export; large domains 
  may require attention
- **Coupling frequency** — For coupled simulations, wind updates every 10-100 fire 
  timesteps are typical

Advanced Topics
---------------

Debugging Export Issues
~~~~~~~~~~~~~~~~~~~~~~~

Enable verbose output to diagnose problems::

    python3 floris_export.py --solver inputs.i --turbines turbines.csv \
        --output wind_data.csv --verbose

Common issues and solutions:

1. **Import errors** — Ensure PYTHONPATH includes the build/python directory::

    export PYTHONPATH=/path/to/massconsistent_amr/build/python:$PYTHONPATH

2. **File not found** — Verify solver inputs and turbine CSV files exist and paths 
   are correct

3. **Interpolation warnings** — Check that turbine locations fall within the 
   computational domain

Custom Export Formats
~~~~~~~~~~~~~~~~~~~~~

You can extend the ``FLORISWindMap`` class to export in additional formats::

    from floris_coupling import FLORISWindMap

    class CustomWindMap(FLORISWindMap):
        def export_to_custom(self, turbines, hub_height, output_file):
            # Implement custom export logic
            pass

Related Files
-------------

- ``src/python/floris_coupling.py`` — Main FLORIS coupling module
- ``src/python/floris_export.py`` — Command-line export tool
- ``src/python/example_floris_export.py`` — Detailed usage examples
- ``tools/floris_export.py`` — Alternative standalone export tool
- ``src/python/wind_solver.py`` — WindSolver Python wrapper

References
----------

- **FLORIS Documentation**: https://nrel.github.io/floris/
- **FLORIS GitHub**: https://github.com/NREL/floris
- **Mass-Consistent Wind Solver**: :ref:`overview`
- **Python API**: :ref:`python_api`
