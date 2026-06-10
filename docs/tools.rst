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
Parses 3D meteorological outputs (WRF, GFS) in NetCDF format, performs coordinate transformations, un-staggers Arakawa-C variables, and interpolates in space and time to produce solver-ready 3D windfield files. See the :ref:`usage guide for complete parameters <usage>`.

HRRR Surface Parameter Extractor (``hrrr_to_surface_data.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Extracts friction velocities (:math:`u_*`), roughness lengths (:math:`z_0`), and 10m velocities from High-Resolution Rapid Refresh (HRRR) GRIB/NetCDF products to build surface parameter CSV files for ``init_mode = surface_data``.

NAM Data Ingestor (``nam_ingestion.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Provides full ingestion of North American Mesoscale (NAM) meteorological datasets under both pathways:
  * **Pathway A (3D Wind Field)**: Extracts and interpolates 3D wind velocity components on the solver coordinate mesh (generates ``windfield.csv`` for ``init_mode = windfield``).
  * **Pathway B (Surface Parameters)**: Extracts friction velocity, roughness, and 10m wind speed/direction (generates ``surface_data.csv`` for ``init_mode = surface_data``).

Climate Projection Downscaler (``download_climate_projection.py``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Queries and downloads future climate projection models (CMIP6, downscaled projections) for target locations.
  * Formats the projected future wind climatology into joint speed-direction distributions (wind roses) to feed directly into the **AEP Calculator**.
  * Outputs wind flow configuration profile files (e.g., ``future_scenarios.ini``) to analyze extreme or representative wind flows under projected future climate regimes.

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

Visualization & Doc Gallery Generators (``tools/postprocessing/``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
A suite of specialized plotting scripts is located under the ``tools/postprocessing/`` subdirectory. These are used to generate the high-resolution publication-quality scenario gallery images in the project documentation and README.

* **Available Scripts**:
  * ``plot_drone_deposition.py``: Generates the agricultural drone spray terrain & deposition map (``docs/drone_deposition_plot.png``).
  * ``plot_terrain_following.py``: Vertical slice wind flow over a Gaussian hill (``docs/terrain_following_complex_flow.png``).
  * ``plot_gorge_bridge.py``: Gorge bridge canyon channeling and wind loading (``docs/gorge_bridge_crossing.png``).
  * ``plot_urban_street_canyon.py``: Urban street canyon channeling and building wakes (``docs/urban_street_canyon.png``).
  * ``plot_transmission_line.py``: Transmission tower/line gap-flow wind loading (``docs/transmission_line_loading.png``).
  * ``plot_turbine_wake.py``: Yawed wind turbine wake deflection (``docs/turbine_wake_deflection.png``).
  * ``plot_valley_amd_hotspots.py``: Valley AMD geochemical hotspots and O₂ delivery (``docs/valley_amd_hotspots.png``).

* **Usage**:
  Refer to the ``tools/postprocessing/README.md`` for complete options and run details.

