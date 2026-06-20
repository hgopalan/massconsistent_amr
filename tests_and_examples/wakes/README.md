# Wind Turbine Wake Scenarios

This directory contains wind farm modeling scenarios, analytical wakes, yaw deflection, and optimization cases.

## Cases & Scripts

### 1. `case4_turbine_wake/`
* **Purpose**: Single turbine wake velocity deficits modeled via the analytical Jensen wake model.

### 2. `iec61400_models/`
* **Purpose**: Simulates extreme wind, gust profiles, and coherent turbulence according to the IEC 61400-1 wind turbine safety standard.

### 3. `happy_jack_wind_farm/`
* **Purpose**: Complex wake modeling for the Happy Jack wind farm under time-varying, orographic wind conditions.

### 4. `pywake_coupling/`
* **Purpose**: Site-level coupling, exporting flow fields from the mass-consistent solver for integration in PyWake.

### 5. `randomized_hill_turbines/`
* **Purpose**: Large-scale simulation of 20 turbines distributed over randomized hill terrain under time-varying cardinal wind directions.

### 6. `wind_farm_tools/`
* **Purpose**: Utilities for layout optimization exports and wind resource summary statistics.
