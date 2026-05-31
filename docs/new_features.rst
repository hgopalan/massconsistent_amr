.. _new_features:

New Features (2026)
===================

This page documents recently added features to the massconsistent_amr wind solver.

Multi-Height Wind Field Extraction
-----------------------------------

**Added:** May 2026  
**Use Case:** Meteorology, wind energy, aviation applications

Overview
~~~~~~~~

The wind solver can now extract wind fields at multiple heights above ground level (AGL)
in a single simulation run. This is particularly useful for:

- Standard meteorological heights (10 m, 100 m, 200 m)
- Wind turbine hub heights (80 m, 120 m, 150 m)
- Aviation analysis at multiple flight levels
- Multi-level validation against weather station data

How to Use
~~~~~~~~~~

In your input file, specify multiple extraction heights using space-separated values::

    # Extract wind at 10m, 50m, 100m, and 200m AGL
    extract_agl  = 10.0 50.0 100.0 200.0
    extract_file = wind_extract.csv

The solver will create separate CSV files for each height:

- ``wind_extract_10m.csv``
- ``wind_extract_50m.csv``
- ``wind_extract_100m.csv``
- ``wind_extract_200m.csv``

Each file contains the standard columns::

    x, y, z_terrain, z_physical, z_agl, u, v, w, speed

Output Format
~~~~~~~~~~~~~

Each extraction file provides:

- **x, y**: Horizontal coordinates [m]
- **z_terrain**: Local terrain elevation [m above sea level]
- **z_physical**: Physical height of the extraction plane [m above sea level]
- **z_agl**: Height above ground level for this grid column [m]
- **u, v, w**: Velocity components [m/s]
- **speed**: Total velocity magnitude [m/s]

Example
~~~~~~~

See the regression test ``regtest/multiheight_extraction/inputs.i`` for a complete
working example.

The test extracts wind at 10 m, 50 m, and 100 m AGL over a Gaussian hill terrain
and verifies that all three output files are created correctly.

Backward Compatibility
~~~~~~~~~~~~~~~~~~~~~~

The feature is fully backward compatible. Single-height extraction still works::

    # Single height extraction (same as before)
    extract_agl  = 15.0
    extract_file = wind_15m.csv

This creates a single file ``wind_15m.csv`` (or ``wind_extract.csv`` if only one height is specified).

Implementation Details
~~~~~~~~~~~~~~~~~~~~~~

- Uses AMReX ``ParmParse::getarr()`` to read array values
- Automatic filename generation with height suffix
- Extraction loop processes all heights efficiently
- GPU-safe implementation with proper synchronization
- MPI-parallel output with sequential file writes

Performance Notes
~~~~~~~~~~~~~~~~~

Extracting multiple heights adds minimal overhead:

- Each extraction is a single k-plane copy operation
- Total runtime increase: ~1-2% per additional height
- I/O is the primary cost (not computation)

For example, extracting 4 heights instead of 1 adds approximately 3-6% to total runtime.

Future Enhancements
~~~~~~~~~~~~~~~~~~~

Potential future additions to this feature:

1. **Cross-section extraction** - Extract YZ or XZ planes at specified coordinates
2. **Time-varying extraction** - Extract at specific time intervals for transient simulations  
3. **Ensemble extraction** - Extract statistics (mean, std dev) over ensemble runs
4. **NetCDF output** - Write extractions in CF-compliant NetCDF format

Related Features
----------------

Coming Soon
~~~~~~~~~~~

The following features are planned for future releases:

1. **Checkpoint/Restart**
   
   - Save solver state for faster parameter sweeps
   - Resume interrupted simulations
   - Expected: Q3 2026

2. **Position-Dependent Roughness**
   
   - Specify spatially-varying surface roughness
   - Support land-use transitions (forest/urban/water)
   - Expected: Q4 2026

3. **Thermal Stratification**
   
   - Monin-Obukhov stability corrections
   - Stable/unstable atmospheric conditions
   - Expected: Q4 2026

Contributing
------------

If you have ideas for new features or improvements, please:

1. Open an issue on GitHub describing the feature
2. Provide use cases and example applications
3. Consider contributing a pull request with implementation

See the project README for contribution guidelines.
