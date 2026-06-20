# Atmospheric Dispersion & Chemical Coupling Scenarios

This directory contains advanced scenarios modeling pollutant transport, aerosol dispersion, and biogeochemical interactions.

## Subfolders & Cases

### 1. `agricultural_drone/` & `colorado_drone_spray/`
* **Purpose**: Simulates aerial spray deposition, rotor downwash, and droplet evaporation from agricultural drones.
* **Physics**: Trajectory pathing, evaporative size shrinkage, downwash-induced deposition, and off-target spray drift analysis.

### 2. `aep_dispersion/`
* **Purpose**: Linearized wake lookups integrated with Puff/LPDM dispersion to model wind farm annual energy production (AEP) alongside localized turbine-wake-induced turbulence dispersion.

### 3. `phreeqc_coupling/`
* **Purpose**: Advanced reactive transport and geochemical coupling using PHREEQC.
* **Physics**:
  * Acid mine drainage (AMD) hotspots and oxygen delivery.
  * Wind-dependent sulfide mineral oxidation rates.
  * Ore leaching efficiency calculations (Sherwood correlation).
  * Temperature-dependent chemical kinetics.
