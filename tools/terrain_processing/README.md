# Terrain Processing Utilities

This directory contains utility scripts for processing, interpolating, and generating topography datasets.

## Utility Scripts

* **`gaussian_hill_generator.py`**: Generates synthetic isolated 3D Gaussian hill topography CSV files for benchmark runs.
* **`terrain_reader_srtm.py`**: Parses 1-arcsecond SRTM HGT files and projects geographic coordinates to local flat/UTM meters.
* **`terrain_interpolator.py`**: Performs high-fidelity spatial interpolation and bilinear smoothing of irregular terrain point clouds onto the solver's structured AMR grids.
