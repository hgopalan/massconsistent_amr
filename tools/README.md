# Wind Solver Utility Tools (`tools`)

This directory contains utility scripts for pre-processing, data ingestion, post-processing, export coupling, and sensitivity analysis/optimization.

## Reorganized Directory Structure

To keep the codebase clean and maintainable, the utility scripts are organized into the following categorical subfolders:

* **[data_ingestion/](./data_ingestion/)**: Meteorological data loaders (ERA5, HRRR, NAM), public elevation/land-use fetchers, and climate projection downscalers.
* **[terrain_processing/](./terrain_processing/)**: Tools for generating synthetic shapes (Gaussian hills), parsing SRTM HGT files, and performing coordinate/spatial interpolations.
* **[export_coupling/](./export_coupling/)**: Flow-field exporters for PyWake, FLORIS, OpenFAST, CALPUFF, and VTK converters for ParaView visualization.
* **[analysis_optimization/](./analysis_optimization/)**: Sensitivity analysis suites, drone operation windows optimization, and turbine layout optimization.
* **[postprocessing/](./postprocessing/)**: Plotting scripts to generate high-resolution, publication-quality scenario figures.
