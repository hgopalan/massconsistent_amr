Implementation Summary: Mass-Consistent Wind Solver Test Cases
==============================================================


Overview
--------


Created 3 comprehensive test cases for the mass-consistent wind solver with time-varying winds, log-law initialization, and OpenFAST synthetic turbulence fluctuation generation.

Components Created
------------------


1. Tools (in tools/)
~~~~~~~~~~~~~~~~~~~~


gaussian_hill_generator.py
^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Purpose**: Generate synthetic Gaussian hill terrain for testing
- **Features**:
  - Configurable grid dimensions (nx, ny)
  - Customizable domain size and peak elevation
  - Adjustable Gaussian width (sigma) parameter
  - CSV output compatible with wind solver
  - Command-line interface with full documentation
- **Class**: ``GaussianHillGenerator`` with methods:
  - ``generate()`` - Create terrain grid
  - ``write_terrain_csv()`` - Export to CSV format
  - ``get_stats()`` - Compute terrain statistics

terrain_reader_srtm.py
^^^^^^^^^^^^^^^^^^^^^^

- **Purpose**: Read SRTM DEM data and convert to wind solver format
- **Features** (Integrated from wildfire_levelset):
  - Parse SRTM 1-arcsecond HGT files
  - Bilinear interpolation for sub-grid accuracy
  - Handle no-data values (-32768)
  - Convert lat/lon to projected coordinates
  - Multi-tile support
  - Command-line interface
- **Classes**:
  - ``SRTMTile`` - Single 1°×1° tile handling
  - ``SRTMReader`` - Multi-tile reader with coordinate transformation

2. Test Cases (in test/)
~~~~~~~~~~~~~~~~~~~~~~~~


Case 1: Gaussian Hill (Synthetic Terrain)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Directory**: ``mass_consistent_case1_gaussian_hill/``

**Terrain**:
- 21×21 grid points over 500×500m domain
- Gaussian hill with 75m peak elevation
- Grid spacing: 25m horizontal

**Input Files**:
- ``inputs.i`` - Wind solver configuration with:
  - Log-law initialization (z0=0.05m)
  - Time-varying winds (10 time steps)
  - OpenFAST turbulence parameters (Von Kármán spectrum)
  - BTS export configuration
- ``terrain_gen.py`` - Terrain generation script
- ``terrain.csv`` - Pre-generated terrain (21×21 Gaussian hill)
- ``time_series.csv`` - Time-varying boundary conditions
- ``test_case1.py`` - Comprehensive test suite

**Test Validations**:
- Solver initialization
- Wind field solution convergence
- Velocity extraction at 30m AGL
- Plotfile output generation
- Terrain field access

Case 2: Flatirons NREL Site (Real SRTM Terrain)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Directory**: ``mass_consistent_case2_flatirons/``

**Terrain**:
- Real SRTM data (Boulder, CO area)
- User-generated from N40W105.hgt tile
- ~3.5 km × 3.5 km domain, 21×21 grid
- Rocky foothills with complex topography

**Input Files**:
- ``inputs.i`` - Configuration with:
  - Log-law initialization (z0=0.1m - grassland)
  - 20 time-varying wind steps
  - TI=0.14, Von Kármán spectrum
  - Wind turbine hub-height extraction (40m AGL)
  - BTS export with 20 time steps
- ``time_series.csv`` - Diurnal wind variation
- ``test_case2.py`` - Test suite with terrain requirement checks

**User Instructions**:
.. code-block:: bash

    # Generate terrain from SRTM data
    python3 ../../tools/terrain_reader_srtm.py N40W105.hgt \
      --output terrain.csv \
      --lat-min 40.010 --lat-max 40.037 \
      --lon-min -105.245 --lon-max -105.218


Case 3: Mt. Hood (Alpine SRTM Terrain)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Directory**: ``mass_consistent_case3_mt_hood/``

**Terrain**:
- Real SRTM data (Mt. Hood, OR area)
- User-generated from N45W121.hgt tile
- Summit area ~4 km × 4 km, high elevation
- Alpine terrain with significant relief

**Input Files**:
- ``inputs.i`` - Configuration with:
  - Log-law initialization (z0=0.2m - alpine vegetation)
  - 25 time-varying wind steps (including gusts)
  - Higher TI=0.16 for complex terrain
  - Von Kármán spectrum
  - Extraction at 50m AGL
  - BTS export with 25 time steps
- ``time_series.csv`` - Wind variation with gust patterns
- ``test_case3.py`` - Test suite for alpine terrain

**User Instructions**:
.. code-block:: bash

    # Generate terrain from SRTM data
    python3 ../../tools/terrain_reader_srtm.py N45W121.hgt \
      --output terrain.csv \
      --lat-min 45.366 --lat-max 45.380 \
      --lon-min -121.696 --lon-max -121.680


3. Wind Solver Enhancement
~~~~~~~~~~~~~~~~~~~~~~~~~~


**File Modified**: ``src/python/wind_solver.py``

**New Methods**:
- ``write_plotfile_with_fluctuations(plotfile_name, fluctuation_file=None)``
  - Writes velocity field with turbulence fluctuations applied
  - Reads fluctuations from BTS file or solver internals
  - Applies fluctuations to corrected wind field
  - Outputs modified wind field to AMReX plotfile
  - Prints fluctuation statistics (RMS values)

- ``_read_bts_fluctuations(bts_file, shape)``
  - Helper method to read turbulence from BTS files
  - Handles binary format parsing
  - Reshapes data to match solver grid
  - Error handling for size mismatches

**Features**:
- Seamless integration with existing solver
- Supports multiple fluctuation sources
- Fallback to corrected field if fluctuations unavailable
- Detailed output statistics

4. Documentation
~~~~~~~~~~~~~~~~


**File**: ``test/README.md``

Comprehensive guide including:
- Overview of all 3 test cases
- Terrain specifications and resolutions
- SRTM data download instructions
- Terrain generation procedures
- Configuration parameter descriptions
- Expected output files and formats
- Build and dependency requirements
- Troubleshooting section
- Test validation checklist

Key Features
------------


OpenFAST Synthetic Turbulence Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


All test cases include full OpenFAST/TurbSim compatible turbulence generation:

**Common Parameters**:
- Spectrum model: Von Kármán (standard in wind energy)
- Intensity model: Power-law with height
- Coherence model: Gaussian exponential decay
- BTS export: Binary format with metadata

**Case-Specific Variations**:
- Case 1: TI=0.12, baseline turbulence
- Case 2: TI=0.14, grassland roughness
- Case 3: TI=0.16, alpine complexity

Time-Varying Boundary Conditions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


All cases include time-varying wind profiles:
- **Case 1**: 10 time steps, simple variation (12-13.5 m/s)
- **Case 2**: 20 time steps, diurnal pattern (11-13 m/s)
- **Case 3**: 25 time steps, with gusts (13-15.7 m/s)

Each time step specifies U, V components or wind speed and direction.

Terrain Flexibility
~~~~~~~~~~~~~~~~~~~


1. **Synthetic (Case 1)**: 
   - Fully parameterizable Gaussian hills
   - Fast generation, reproducible
   - Ideal for CI/regression testing

2. **Real (Cases 2 & 3)**:
   - SRTM data with ~30m resolution
   - User provides terrain generation
   - Realistic wind flow patterns
   - Validation against real sites

Usage Examples
--------------


Case 1 - Run Immediately
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    cd test/mass_consistent_case1_gaussian_hill
    python3 test_case1.py


Case 2 - With Terrain Generation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    cd test/mass_consistent_case2_flatirons
    # Generate terrain (one-time setup)
    python3 ../../tools/terrain_reader_srtm.py N40W105.hgt \
      --output terrain.csv \
      --lat-min 40.010 --lat-max 40.037 \
      --lon-min -105.245 --lon-max -105.218
    # Run test
    python3 test_case2.py


Case 3 - High Elevation Testing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    cd test/mass_consistent_case3_mt_hood
    # Generate terrain (one-time setup)
    python3 ../../tools/terrain_reader_srtm.py N45W121.hgt \
      --output terrain.csv \
      --lat-min 45.366 --lat-max 45.380 \
      --lon-min -121.696 --lon-max -121.680
    # Run test
    python3 test_case3.py


Test Validation
---------------


Each test case validates:

✓ Terrain file existence and format  
✓ Solver initialization with correct grid dimensions  
✓ Wind field solution convergence (MLMG iterations)  
✓ Velocity field extraction at specific AGL heights  
✓ Physical parameter ranges (wind speed, turbulence)  
✓ Plotfile generation (with and without fluctuations)  
✓ BTS export for OpenFAST compatibility  

Output Files Generated
----------------------


Per Case:
~~~~~~~~~

- ``plt_case#_winds/`` - Corrected wind field (AMReX plotfile)
- ``plt_case#_winds_with_fluctuations/`` - Wind + turbulence (AMReX plotfile)
- ``wind_extract*.csv`` - 2D wind field at AGL height
- ``case#_turbulence.bts`` - Binary BTS file for OpenFAST
- ``case#_turbulence.meta`` - Metadata file

Build Requirements
------------------


.. code-block:: bash

    cmake -S . -B build \
      -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON \
      -DMASSCONSISTENT_ENABLE_OPENFAST=ON
    cd build && make -j4


Dependencies
------------


**Python Modules**:
- numpy (for array operations)
- struct (for BTS binary format - standard library)

**External Data** (Cases 2 & 3):
- SRTM HGT files (~25 MB each)
- Available from USGS SRTM server

Architecture
------------


.. code-block:: text

    massconsistent_amr/
    ├── tools/
    │   ├── gaussian_hill_generator.py    ← New: Gaussian terrain generation
    │   ├── terrain_reader_srtm.py        ← New: SRTM DEM reader
    │   └── openfast_export.py            ← Existing: BTS export
    ├── src/python/
    │   ├── wind_solver.py                ← Modified: Added write_plotfile_with_fluctuations()
    │   └── pyWindSolver.cpp              ← C++ bindings (unchanged)
    ├── test/                             ← New: All test cases
    │   ├── README.md
    │   ├── mass_consistent_case1_gaussian_hill/
    │   │   ├── test_case1.py
    │   │   ├── inputs.i
    │   │   ├── terrain_gen.py
    │   │   ├── terrain.csv
    │   │   └── time_series.csv
    │   ├── mass_consistent_case2_flatirons/
    │   │   ├── test_case2.py
    │   │   ├── inputs.i
    │   │   └── time_series.csv
    │   └── mass_consistent_case3_mt_hood/
    │       ├── test_case3.py
    │       ├── inputs.i
    │       └── time_series.csv


Integration Points
------------------


1. **PyWindSolver API**: Uses existing ``get_velocity()``, ``get_velocity_at_agl()``, ``write_plotfile()`` functions
2. **OpenFAST Export**: Leverages existing BTS writer for turbulence output
3. **SRTM Support**: Integrates terrain reader from wildfire_levelset project
4. **Time-Varying Winds**: Uses existing boundary condition system

Next Steps for Users
--------------------


1. **Case 1**: Ready to use immediately
.. code-block:: bash

       python3 test/mass_consistent_case1_gaussian_hill/test_case1.py


2. **Cases 2 & 3**: Requires SRTM data download and terrain generation
   - Download from USGS SRTM server
   - Use ``terrain_reader_srtm.py`` to generate terrain.csv
   - Then run test suite

3. **Customization**: Modify inputs.i files to test different parameters:
   - Grid spacing (dx, dy, dz)
   - Domain height
   - Turbulence parameters (spectrum, intensity, scales)
   - BTS export settings

References
----------


- **Mass-consistent solver**: AMReX-based atmospheric dynamics
- **SRTM**: Shuttle Radar Topography Mission (USGS)
- **OpenFAST**: NREL wind turbine simulation tool
- **Von Kármán spectrum**: Standard turbulence model in wind energy
