# pyWindSolver - Python Bindings for massconsistent_amr

Provides the pyWindSolver extension module and high-level wrapper classes to control the mass-consistent wind solver.

## Modules

### Core Wind Solver
- **wind_solver.py** - High-level Python wrapper for the C++ mass-consistent wind solver
- **pyWindSolver.cpp** - C++ Python bindings

### Analysis Tools

#### Data Center Siting Tool
- **datacenter_siting.py** - Multi-criteria siting optimization for data center deployment
  - Comprehensive climate characterization (wind, temperature, humidity)
  - Cooling efficiency evaluation (free cooling hours, ambient conditions)
  - Infrastructure resilience assessment (wind extremes, flood risk)
  - Environmental impact quantification (heat island, air quality)
  - Multi-criteria scoring and Pareto frontier analysis
  - Supports multiple priority profiles: BALANCED, COOLING_EFFICIENCY, RESILIENCE, ENVIRONMENTAL, COST_OPTIMIZED
  
  Example:
  ```python
  from datacenter_siting import SitingAnalyzer, CandidateSite, SitingPriority
  
  sites = [
      CandidateSite("site_a", x=100000, y=200000, label="Mountain Valley"),
      CandidateSite("site_b", x=150000, y=250000, label="Coastal Plain"),
  ]
  
  analyzer = SitingAnalyzer(sites, priority=SitingPriority.COOLING_EFFICIENCY)
  evaluations = analyzer.evaluate_all_sites()
  analyzer.generate_report("siting_report.json", "siting_scores.csv")
  analyzer.plot_results("scores.png", "pareto_frontier.png")
  ```

#### Infrastructure & Wind Farm Analysis
- **infrastructure_models.py** - Bridge/structure loading and wind comfort assessment
- **aep_calculator.py** - Wind farm Annual Energy Production calculation
- **iec61400_models.py** - IEC 61400 wind turbine design load cases
- **mann_box.py** - Mann Box turbulence synthesis for OpenFAST/TurbSim

### External Couplings
- **floris_coupling.py** - Integration with FLORIS wind farm optimization
- **pywake_coupling.py** - Integration with PyWake wake model
- **levelset_coupling.py** - Two-way coupling with wildfire_levelset fire solver
  - One-way coupling: Wind field drives fire spread
  - Two-way coupling: Fire heating affects wind dynamics
- **agricultural_drone.py** - Agricultural drone spray drift and deposition modeling
- **phreeqc_coupling/** - Geochemical reactive transport coupling with PHREEQC

### Examples
- **example_floris_export.py** - Wind field export to FLORIS format
- **example_iec61400_models.py** - IEC 61400 design load case application
- **example_openfast_export.py** - OpenFAST turbulence export
- **example_datacenter_siting.py** - Data center multi-site siting analysis

## Wildfire-Wind Coupling

The `levelset_coupling` module enables coupling between massconsistent_amr and wildfire_levelset for integrated fire-atmosphere simulations.

### One-way Coupling (Wind → Fire)
Wind field is computed independently and drives fire spread. Fire does NOT affect wind.

```python
from wind_solver import WindSolver
from levelset_coupling import CoupledWindFireSimulation

# Create coupled solver
coupled = CoupledWindFireSimulation(
    wind_inputs="wind_inputs.i",
    fire_inputs="fire_inputs.i",
    coupling_mode='one_way'
)

# Run simulation
coupled.run(final_time=3600.0, wind_update_interval=1)
coupled.finalize()
```

### Two-way Coupling (Wind ↔ Fire)
Wind field is computed with fire heating effects. Fire heating is fed back to wind solver.

```python
from levelset_coupling import CoupledWindFireSimulation

# Create coupled solver in two-way mode
coupled = CoupledWindFireSimulation(
    wind_inputs="wind_inputs.i",
    fire_inputs="fire_inputs.i",
    coupling_mode='two_way'
)

# Run simulation with heat source feedback
coupled.run(
    final_time=3600.0,
    wind_update_interval=1,
    plot_interval=600.0,
    callback=None
)
coupled.finalize()
```

### Heat Source Management
The wind solver now supports heat source injection for two-way coupling:

```python
from wind_solver import WindSolver
import numpy as np

wind = WindSolver("wind_inputs.i")

# Add heat source (e.g., from fire)
heat_flux = np.zeros((wind.ny, wind.nx))
# ... populate heat_flux from fire simulation ...
wind.add_heat_source(heat_flux)

# Solve with heat source
wind.solve()

# Check heat source status
status = wind.get_heat_source()
print(f"Heat source active: {status['is_active']}")
```

### Domain Requirements
For proper coupling:
- **Horizontal domains must match:** `xmin, xmax, ymin, ymax` must be identical
- **Grid spacing must match:** `dx, dy` should be equal
- **Vertical levels:** Fire solver is 2D; wind solver is 3D (nz levels)

The coupling module automatically checks domain compatibility and warns if mismatches are detected.

### References
- **massconsistent_amr:** https://github.com/hgopalan/massconsistent_amr
- **wildfire_levelset:** https://github.com/hgopalan/wildfire_levelset
- **Regression tests:** `regtest/fire_coupling/`

