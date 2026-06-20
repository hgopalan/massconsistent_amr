# Data Ingestion Utilities

This directory contains meteorological data loaders and geographic data downloaders for pre-processing.

## Utility Scripts

* **`geographic_data_fetcher.py`**: Queries public web APIs to automatically download elevation DEMs (e.g. SRTM/USGS 3DEP) and land-use (e.g. USGS NLCD) maps.
* **`netcdf_to_windfield.py`**: Parses 3D WRF or GFS outputs in NetCDF, un-staggers variables, and generates solver-ready `windfield.csv`.
* **`hrrr_to_surface_data.py`**: Extracts friction velocities and roughness from HRRR files for surface-based initialization.
* **`nam_ingestion.py`**: Full NAM meteorological dataset loader supporting 3D wind fields and surface parameters.
* **`download_climate_projection.py`**: Downloads and downscales CMIP6 climate models for future wind rose and extreme-wind return calculations.
* **`farsite_weather_reader.py`**: Parses weather schedules from FARSITE format for wind-fire coupling.
