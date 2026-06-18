Wind Farm Tools — File Format Compatibility & Data Exchange
==============================================================

This directory contains demonstrations and tests for wind farm compatibility utilities that enable
file format compatibility and data exchange with Floris and other wind farm
optimization tools.

## Utilities Demonstrated

### CSV Turbine Definition Format
Provides read/write capabilities for turbine layouts in standardized CSV format.

**Files:**
- `turbine_io.py` (src/python/) - Core module

**Capabilities:**
- Load turbine positions from CSV
- Write turbine layouts to CSV
- Validate turbine spacing and domain bounds
- Support for heterogeneous turbine types

**Example CSV Format:**
```
turbine_id, x_m, y_m, z_agl_m, turbine_type, hub_height, rotor_diameter, power_curve_file
0, 100.0, 200.0, 0.0, DTU10MW, 90.0, 178.0, power_curves/dtu10mw.json
1, 500.0, 200.0, 50.0, NREL15MW, 120.0, 240.0, power_curves/nrel15mw.json
```

### Wind Resource Summary Statistics
Computes statistical summaries of wind fields for resource assessment.

**Files:**
- `wind_resource_stats.py` (src/python/) - Core module

**Capabilities:**
- Mean, std dev, min/max wind speed
- Wind direction statistics (circular mean)
- Weibull distribution parameters (k, c)
- Wind rose statistics
- JSON export

**Statistics Computed:**
- Mean wind speed and direction
- Turbulence intensity indicators
- Weibull shape (k) and scale (c) parameters
- Spatial distribution characteristics

### PyOptimization Result Export
Exports wind farm simulation results in formats compatible with Floris-PyOptimization.

**Files:**
- `pyoptimization_export.py` (src/python/) - Core module

**Capabilities:**
- JSON export (PyOptimization-compatible format)
- Per-turbine CSV export (power, wind conditions, Ct, yaw)
- Farm summary CSV export
- Wind resource metadata

**Output Formats:**
```
{
  "metadata": {
    "farm_name": "Wind_Farm",
    "version": "1.0"
  },
  "farm_summary": {
    "num_turbines": 4,
    "total_power_kw": 16200.0,
    "annual_energy_gwh": 141.9
  },
  "turbines": [...]
}
```

## Running the Examples

### Run demonstration:
```bash
cd tests_and_examples/phase1_features
python3 test_phase1_wind_farm.py
```

This will:
1. Create a turbine layout and export to CSV
2. Generate synthetic wind field and compute statistics
3. Export results in PyOptimization format
4. Display sample outputs

### Run unit tests:
```bash
cd src/python
python3 test_phase1_features.py -v
```

This executes comprehensive unit tests for all three features.

## Generated Files

Running the example produces:
- `wind_farm_turbines.csv` - Turbine layout
- `wind_farm_stats.json` - Wind statistics
- `wind_farm_results.json` - PyOptimization results (JSON)
- `wind_farm_turbine_results.csv` - Per-turbine results (CSV)
- `wind_farm_farm_summary.csv` - Farm-level summary (CSV)

## Usage in Custom Scripts

### CSV Turbine Layout

```python
from turbine_io import TurbineLayout

# Create and populate layout
layout = TurbineLayout()
layout.add_turbine(0, 100.0, 200.0, hub_height=90.0)
layout.add_turbine(1, 500.0, 200.0, hub_height=90.0)

# Write to CSV
TurbineLayout.write_csv(layout, "turbines.csv")

# Read from CSV
layout = TurbineLayout.read_csv("turbines.csv")

# Validate spacing
is_valid, errors = layout.validate_spacing(min_spacing=400.0)
```

### Wind Statistics

```python
from wind_resource_stats import WindResourceStats
import numpy as np

# Compute from wind field
u_field = np.ones((10, 10)) * 10.0
v_field = np.zeros((10, 10))
stats = WindResourceStats.compute_from_wind_field(u_field, v_field, height_agl=90.0)

# Access statistics
print(f"Mean speed: {stats.mean_speed:.2f} m/s")
print(f"Weibull k: {stats.weibull_k:.2f}")

# Export to JSON
stats.to_json("wind_stats.json")
```

### PyOptimization Export

```python
from pyoptimization_export import PyOptimizationExporter

# Create exporter
exporter = PyOptimizationExporter("My_Farm")

# Add turbine results
exporter.add_turbine_result(
    turbine_id=0, x=100.0, y=200.0,
    power_kw=4000.0, wind_speed_ms=10.0, wind_direction_deg=270.0
)

# Set farm aggregates
exporter.set_farm_power(4000.0, annual_energy_gwh=35.0)

# Export
exporter.export_json("results.json")
exporter.export_turbine_csv("turbines.csv")
```

## Integration Notes

These utilities enable:
- **Data interoperability** with Floris and PyOptimization
- **Standardized input formats** for turbine layouts
- **Wind resource assessment** from simulation results
- **Result archival** in portable formats (CSV, JSON)

Future enhancements will include:
- Farm analytics and visualization tools
- Advanced optimization frameworks
- Control strategies and uncertainty quantification

## Dependencies

- Python 3.7+
- NumPy (for statistics)
- SciPy (for Weibull fitting)
- Standard library: csv, json

## References

- Turbine layout management: CSV I/O best practices
- Wind statistics: IEC 61400-1 resource assessment
- PyOptimization compatibility: Floris v3 JSON schema
