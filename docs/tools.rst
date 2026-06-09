.. _tools:

Tools and Utilities
===================

This section documents the pre-processing, terrain manipulation, data ingestion, and post-processing Python utility scripts located in the ``tools/`` directory.

.. contents:: Topics
   :local:
   :depth: 2

Pre-Processing & Terrain Utilities
----------------------------------

Gaussian Hill Generator (``gaussian_hill_generator.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Generates synthetic Gaussian hill topography CSV files for testing, validation, and benchmarking.

* **Usage**:

  .. code-block:: bash

     python3 tools/gaussian_hill_generator.py \
       --nx 21 --ny 21 \
       --dx 25.0 --dy 25.0 \
       --peak-elevation 75.0 \
       --sigma 100.0 \
       --output terrain.csv

* **Options**:
  * ``--nx, --ny``: Number of grid cells in x and y.
  * ``--dx, --dy``: Cell resolution [m].
  * ``--peak-elevation``: Peak height [m] of the Gaussian hill.
  * ``--sigma``: Width parameter [m] of the hill shape.
  * ``--output``: Destination file path.

SRTM Terrain Reader (``terrain_reader_srtm.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Parses Shuttle Radar Topography Mission (SRTM) 1-arcsecond HGT files and projects coordinate systems to local flat/UTM grids, exporting solver-compatible CSV terrain point clouds.

* **Usage**:

  .. code-block:: bash

     python3 tools/terrain_reader_srtm.py N45W121.hgt \
       --output terrain.csv \
       --lat-min 45.36 --lat-max 45.38 \
       --lon-min -121.70 --lon-max -121.68

* **Key Features**:
  * Dual-tile seamless bilinear interpolation.
  * Correct coordinate projections from geographic (latitude/longitude) to local UTM meters.
  * Handles void/missing values safely.

Geographic Data Fetcher (``geographic_data_fetcher.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Queries public web APIs (such as USGS or NASA databases) using latitude/longitude bounding boxes to automatically download and format elevation DEMs (e.g., SRTM or USGS 3DEP) and land-cover maps (e.g., USGS NLCD) into solver-compatible local flat/UTM grid coordinate formats.

* **Usage**:

  .. code-block:: bash

     python3 tools/geographic_data_fetcher.py \
       --lat-min 39.9 --lat-max 40.1 \
       --lon-min -105.3 --lon-max -105.2 \
       --nx 100 --ny 100 \
       --dem-output terrain.csv \
       --lc-output landuse.csv \
       --projection flat

* **Key Features**:
  * Seamlessly fetches elevation grids from USGS 3DEP or OpenTopography SRTM endpoints.
  * Fetches USGS NLCD land-use maps to automatically generate local surface roughness CSV outputs matching the grid spacing.
  * Dual-projection system supporting local relative UTM meters or flat-earth projections.
  * Advanced high-quality offline mockup/synthetic data generation when running in restricted or sandboxed environments without network access.

Boundary Condition & Weather Processing
---------------------------------------

NetCDF Wind Field Parser (``netcdf_to_windfield.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Parses 3D meteorological outputs (WRF, GFS) in NetCDF format, performs coordinate transformations, un-staggers Arakawa-C variables, and interpolates in space and time to produce solver-ready 3D windfield files. See the :ref:`usage` guide for complete parameters.

HRRR Surface Parameter Extractor (``hrrr_to_surface_data.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Extracts friction velocities (:math:`u_*`), roughness lengths (:math:`z_0`), and 10m velocities from High-Resolution Rapid Refresh (HRRR) GRIB/NetCDF products to build surface parameter CSV files for ``init_mode = surface_data``.

FARSITE Weather Parser (``farsite_weather_reader.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Reads diurnal, multi-station weather schedules from FARSITE format files, formatting them for coupled wind-fire time-loop simulation steering.

Post-Processing & Export Utilities
----------------------------------

BTS to VTK Converter (``bts_to_vtk.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Converts binary OpenFAST/TurbSim synthetic turbulence BTS files into structured XML VTK files (``.vts`` or ``.vtu``) for visual profiling and analysis in ParaView or VisIt.

* **Usage**:

  .. code-block:: bash

     python3 tools/bts_to_vtk.py input_file.bts output_file.vtk

FLORIS Export Driver (``floris_export.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Samples converged 3D solver MultiFabs at discrete wind-turbine coordinates and hub heights to export FLORIS-compatible wind speeds and speed-up ratio JSON/CSV tables.

OpenFAST Standalone Export (``openfast_export.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Standalone serializing driver that packages spatial-temporal velocity fluctuation fields into TurbSim-compliant binary layouts. Handles header specifications, coordinate layouts, and grid metadata.
