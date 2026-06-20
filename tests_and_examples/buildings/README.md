# Buildings & Urban Canopy Scenarios

This directory contains scenarios and verification cases for flows around urban structures, building wakes, and complex street canyon geometries.

## Subfolders & Cases

### 1. `building_wake/`
* **Purpose**: Verification of building-induced wake models.
* **Physics**: Applies the Röckle, Huber-Snyder, and AERMOD PRIME wake model parameterizations.
* **Execution**: Run `run_verification.py` to compare wake shapes, velocity deficits, and near-wake recirculations.

### 2. `flatirons_buildings_svf/`
* **Purpose**: Flatirons urban building geometry with Sky View Factor (SVF) evaluations.
* **Execution**: Run `test_flatirons_buildings_svf.py` to test SVF estimations around building shapes in complex terrain.

### 3. `urban_heat_island_building/`
* **Purpose**: Scenario generation for micro-climate thermal modeling and building heat flux.
* **Execution**: Run `scenario_generator.py` to produce building and surface heat flux boundary conditions.
