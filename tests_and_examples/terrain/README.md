# Terrain Modeling Scenarios

This directory contains examples of wind solver runs over real and synthetic terrain.

## Cases & Scripts

### 1. `case1_gaussian_hill/`
* **Purpose**: Classic verification case of flow over a single isolated 3D Gaussian hill.
* **Execution**: Run `test_case1.py` or generate customized geometry with `terrain_gen.py`.

### 2. `multi_gaussian_hill/`
* **Purpose**: Simulates complex nested and overlapping Gaussian hills to evaluate solver robustness over undulating topographies.

### 3. `case2_flatirons/`
* **Purpose**: High-fidelity flow simulation over the Boulder Flatirons (CO) using real DEM data.
* **Execution**: Run `test_case2.py` (requires fetching Flatirons elevation data).

### 4. `case3_mt_hood/`
* **Purpose**: Extreme alpine flow terrain simulation over Mt. Hood (OR) under time-varying wind boundary conditions.
* **Execution**: Run `test_case3.py` (requires Mt. Hood SRTM dataset).

### 5. `geographic_data_fetching/`
* **Purpose**: Unit tests for automated DEM and land-use downloading APIs.
* **Execution**: Run `test_geographic_data_fetching.py`.

### 6. `terrain_aware_masking/`
* **Purpose**: Evaluates grid cell masking transitions below complex topographic boundaries.
