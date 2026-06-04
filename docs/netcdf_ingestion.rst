3D Meteorological Data Ingestion (NetCDF/GRIB Parser)
=====================================================

This feature supports the initialization of the 3D wind field from external meteorological models (like WRF or GFS outputs) through Python parsing, terrain-aware spatial mapping/interpolation, and time interpolation.

Parser Utility
--------------

The Python-based parser utility is located at `tools/netcdf_to_windfield.py`. It converts meteorological 3D wind fields in NetCDF files to a 3D windfield CSV file (`windfield.csv`) formatted specifically for the solver grid.

Features:
* **Terrain-Aware Spatial Interpolation:** It maps the source meteorological grid to the solver's coordinate system relative to the height above ground level (AGL). This preserves the planetary boundary layer structure across different terrains.
* **Time Interpolation:** It parses multiple NetCDF time instances or files, sorts them, and interpolates in time for a specified target initial condition time.
* **Support for Standard Datasets:** Automatically unstaggers WRF variables (staggered on Arakawa-C grids) or handles standard Cartesian coordinates.

Usage
-----

To run the parser utility, specify the input files, the target solver configuration (`inputs.i`), and the target time:

.. code-block:: bash

   python3 tools/netcdf_to_windfield.py \
     --nc-files wrf_t1.nc wrf_t2.nc \
     --inputs inputs.i \
     --output windfield.csv \
     --time 50.0

Alternatively, you can provide a text file listing the input NetCDF files:

.. code-block:: bash

   python3 tools/netcdf_to_windfield.py \
     --file-list file_list.txt \
     --inputs inputs.i \
     --output windfield.csv

Solver Configuration
--------------------

To initialize the C++ wind solver using the pre-processed wind field CSV, update the solver configuration (`inputs.i`):

.. code-block:: ini

   # Initialization mode: "windfield" reads from pre-mapped CSV data
   init_mode = windfield
   windfield_file = windfield.csv

The solver will load the coordinates and 3D velocity components (U, V, W) from the windfield CSV, apply a 3D inverse distance weighting (IDW) interpolation to fill the cell-centered velocities, and apply the mass-consistency Poisson adjustment.
