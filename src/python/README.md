# Python Bindings for massconsistent_amr

This directory contains Python bindings for the mass-consistent wind solver and utilities for coupling with external models including wind farm simulators (FLORIS, PyWake) and geochemical solvers (PHREEQC).

## Quick Start: Wind Solver

```python
from wind_solver import WindSolver

# Create solver instance
wind = WindSolver()

# Initialize from inputs file
wind.initialize("inputs.i")

# Solve for wind field
result = wind.solve()

# Extract velocity field
vel = wind.get_velocity()

# Extract velocity at specific height
vel_30m = wind.get_velocity_at_agl(30.0)

# Cleanup
wind.finalize()
```

## Reactive Transport Coupling (PHREEQC Integration)

The solver can export atmospheric boundary conditions for reactive transport simulations in PHREEQC, enabling analysis of:
- **Acid Mine Drainage (AMD)**: Wind-modulated oxidation rates in valley terrain
- **Mineral Weathering**: Terrain-driven atmospheric heterogeneity driving localized dissolution
- **Critical Mineral Leaching**: Wind-dependent mass transfer in ore processing

Example - AMD hotspot identification:

```python
from wind_solver import WindSolver
from reactive_transport_coupling import ReactiveTransportCoupling

# Solve wind field
wind = WindSolver("inputs.i")
wind.solve()

# Initialize coupling
coupling = ReactiveTransportCoupling(wind)

# Identify AMD hotspots based on oxygen delivery
hotspots = coupling.compute_amd_hotspot_map(output_dir="hotspots/")

# Generate PHREEQC input for reactive transport
amd_result = coupling.run_amd_simulation(
    output_dir="amd_simulation/",
    run_phreeqc=False  # Set to True if PHREEQC installed
)

wind.finalize()
```

Key capabilities:
- **Field extraction**: Temperature, pressure, humidity, turbulent diffusivity, atmospheric stability
- **Boundary condition export**: Supports PHREEQC reactive transport with wind-resolved mixing
- **NetCDF I/O**: Efficient data serialization for external analysis
- **Hotspot analysis**: Identify geochemical "hotspots" driven by topographic wind steering

References:
- Parkhurst & Appelo (2013). Description of the PHREEQC-3 software. USGS Techniques Methods
- Businger et al. (1971). Flux-profile relationships in the atmospheric surface layer. J. Atmos. Sci., 28(2)

## Wind Farm Coupling

Export wind fields for wind farm simulators:

```python
from wind_solver import WindSolver
from floris_coupling import quick_export

wind = WindSolver("inputs.i")
wind.solve()
turbines = [(100, 200), (300, 400)]
wind_data = quick_export(wind, turbines, hub_height=90.0)
wind.finalize()
```

## Building

To build with Python bindings enabled:

```bash
cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build
```

## Installation

After building, the module is available in `build/python/`. To use it, add the path to `PYTHONPATH`:

```bash
export PYTHONPATH=${PWD}/build/python:$PYTHONPATH
```

Optional dependencies for extended functionality:
- `netcdf4`: For efficient NetCDF atmospheric field I/O (install via `pip install netcdf4`)
- `phreeqc` or `phreeqc.py`: For executing coupled PHREEQC simulations

## Modules

- **wind_solver.py**: High-level WindSolver class
- **geochemical_coupling.py**: Field extraction for PHREEQC
- **reactive_transport_coupling.py**: Main coupling interface
- **phreeqc_utils.py**: PHREEQC input file generation
- **netcdf_io.py**: NetCDF and ASCII I/O
- **floris_coupling.py**: FLORIS wind farm integration
- **pywake_coupling.py**: PyWake wind farm integration
