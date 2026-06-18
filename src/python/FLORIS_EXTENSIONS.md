# FLORIS Wind Farm Integration Extensions

Advanced FLORIS export and integration utilities for wind farm simulation and analysis.

## Modules

### FLORISConfigExporter
Generates native FLORIS configuration files in JSON format from wind farm layouts.

**Usage:**
```python
from floris_extensions import FLORISConfigExporter

config = FLORISConfigExporter.export_farm_config(
    turbine_locations=[(300, 300), (800, 300)],
    hub_heights=[90, 90],
    rotor_diameters=[178, 178],
    wind_speed=10.0,
    wind_direction=270.0,
    output_file="farm_config.json"
)
```

### EnhancedCSVExporter
Exports wind data with comprehensive meteorological metadata including turbulence intensity, wind shear exponent, and air density.

**Output columns:**
- `turbine_id`, `x_m`, `y_m`: Location
- `z_terrain_m`, `z_hub_m`: Elevation
- `u_ms`, `v_ms`, `wind_speed_ms`, `wind_direction_deg`: Wind components
- `hub_height_agl_m`: Turbine hub height
- `turbulence_intensity`: Atmospheric turbulence
- `wind_shear_exponent`: Wind profile exponent
- `air_density_kg_m3`: Atmospheric density
- `power_output_w`: Turbine power
- `yaw_angle_deg`: Yaw control angle

**Usage:**
```python
from floris_extensions import EnhancedCSVExporter

EnhancedCSVExporter.export_with_metadata(
    turbine_locations=turbine_locs,
    wind_data=wind_at_turbines,
    hub_heights=[90, 90, 90],
    output_file="enhanced_wind_data.csv"
)
```

### PowerCurveGenerator
Generates FLORIS-compatible power and thrust coefficient curves from simulation results.

**Usage:**
```python
from floris_extensions import PowerCurveGenerator

curve = PowerCurveGenerator.generate_from_turbine_data(
    turbine_id=0,
    wind_speeds=[3, 5, 7, 9, 11, 13, 15, 20, 25],
    power_outputs=[0, 100k, 500k, 1000k, 1500k, 1800k, 2000k, 2000k, 0],
    thrust_coefficients=[0.8, 0.78, 0.75, 0.70, 0.60, 0.40, 0.20, 0.05, 0.0],
    output_file="power_curve.json"
)
```

### WindRoseFormatter
Exports wind resources as frequency distributions (wind rose) for FLORIS wind resource analysis.

**Features:**
- Creates 2D histogram of wind speed vs. direction
- Generates frequency matrix suitable for FLORIS
- Exports both JSON and CSV formats

**Usage:**
```python
from floris_extensions import WindRoseFormatter

wind_rose = WindRoseFormatter.create_from_simulation(
    wind_speeds=annual_wind_speeds,
    wind_directions=annual_wind_directions,
    output_file="wind_rose.json"
)

WindRoseFormatter.export_frequency_table(
    wind_rose_data=wind_rose,
    output_file="wind_rose_frequencies.csv"
)
```

### YawOptimizationFormatter
Formats yaw control optimization results for FLORIS implementation.

**Usage:**
```python
from floris_extensions import YawOptimizationFormatter

results = YawOptimizationFormatter.export_yaw_results(
    yaw_angles=[0, 5, -3, 8, -2],
    wind_speed=10.0,
    wind_direction=270.0,
    improvement_pct=3.5,
    output_file="yaw_optimization.json"
)

# Or export as complete farm config with yaw control
config = YawOptimizationFormatter.export_yaw_control_config(
    yaw_angles=[0, 5, -3, 8, -2],
    turbine_locations=turbine_locs,
    hub_heights=[90]*5,
    rotor_diameters=[100]*5,
    output_file="farm_with_yaw_control.json"
)
```

## Quick Export

Export all formats in a single function call:

```python
from floris_extensions import quick_floris_export

files = quick_floris_export(
    turbine_locations=turbine_locs,
    wind_data=wind_at_turbines,
    hub_heights=[90, 90, 90],
    output_prefix="floris_export"
)
# Returns: {'config': 'floris_export_config.json', 'csv': 'floris_export_enhanced.csv'}
```

## Integration with massconsistent_amr

All modules are designed to work seamlessly with massconsistent_amr wind field outputs.

### Typical workflow:

1. **Solve wind field** using massconsistent_amr
2. **Extract wind data** at turbine locations using FLORISWindMap
3. **Export configurations** using FLORIS extension modules
4. **Load in FLORIS** for wind farm simulation and optimization

### Example:

```python
from wind_solver import WindSolver
from floris_coupling import FLORISWindMap
from floris_extensions import FLORISConfigExporter, EnhancedCSVExporter

# Solve wind field
wind = WindSolver("inputs.i")
wind.solve()

# Extract wind at turbines
wind_map = FLORISWindMap(wind)
turbine_locs = [(300, 300), (800, 300), (1300, 300)]
winds = wind_map.get_wind_at_turbines(turbine_locs, hub_height=90.0)

# Export for FLORIS
FLORISConfigExporter.export_farm_config(
    turbine_locations=turbine_locs,
    hub_heights=[90]*3,
    rotor_diameters=[100]*3,
    output_file="farm.json"
)

EnhancedCSVExporter.export_with_metadata(
    turbine_locations=turbine_locs,
    wind_data=winds,
    hub_heights=[90]*3,
    output_file="wind_data.csv"
)

wind.finalize()
```

## File Formats

### FLORIS Native Configuration (JSON)
Complete farm configuration for direct loading into FLORIS:
- Farm layout (turbine x/y positions)
- Turbine properties (hub height, rotor diameter)
- Wind conditions (speed, direction, turbulence)
- Control settings (yaw angles, wake model)

### Enhanced Wind Data (CSV)
Comprehensive wind and turbine data at each location:
- 3D wind components and derived metrics
- Meteorological properties
- Turbine performance
- Control inputs

### Power Curves (JSON)
Turbine performance curves for FLORIS power calculations:
- Wind speed bins
- Power output at each speed
- Thrust coefficient at each speed

### Wind Rose (JSON/CSV)
Wind resource statistics as frequency distributions:
- 2D histogram of wind speed vs. direction
- Frequency matrix for each bin
- Summary statistics

### Yaw Optimization (JSON)
Yaw control results and configurations:
- Optimized yaw angles per turbine
- Wind condition (speed, direction)
- Power improvement estimate
- Complete farm config with yaw angles applied

## Requirements

- Python 3.6+
- numpy (optional, required only for WindRoseFormatter with histogram binning)
- Standard library: json, csv, pathlib

## See Also

- `floris_coupling.py` - Basic FLORIS wind map export
- `pywake_coupling.py` - PyWake integration
- `aep_calculator.py` - Annual energy production calculations
