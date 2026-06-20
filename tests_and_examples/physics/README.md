# Physical Process & Assimilation Scenarios

This directory contains validation cases focusing on boundary layer physics, complex local anisotropy, and meteorological data assimilation.

## Cases & Scripts

* **`cell_local_anisotropy_complex/`**:
  Validates localized anisotropy tensors over steep, complex topography, ensuring numerical stability and correct pressure corrections in the mass-consistent solver.
* **`advanced_bl_and_assimilation/`**:
  Tests spatially varying atmospheric boundary layer (ABL) heights and 3D wind velocity assimilation from profiling stations.
* **`nwp_terrain_options/`**:
  Validates flow physics coupling with numerical weather prediction (NWP) terrain options.
* **`altamont_pass_transmission/`**:
  Wind loading and terrain canyon channeling scenarios modeled specifically for high-voltage transmission lines in the Altamont Pass.
* **`gorge_bridge_crossing/`**:
  Channeling and canyon flow speed-ups near deep gorge bridges.
