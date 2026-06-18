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
- **agricultural_drone.py** - Agricultural drone spray drift and deposition modeling
- **phreeqc_coupling/** - Geochemical reactive transport coupling with PHREEQC

### Examples
- **example_floris_export.py** - Wind field export to FLORIS format
- **example_iec61400_models.py** - IEC 61400 design load case application
- **example_openfast_export.py** - OpenFAST turbulence export
- **example_datacenter_siting.py** - Data center multi-site siting analysis
