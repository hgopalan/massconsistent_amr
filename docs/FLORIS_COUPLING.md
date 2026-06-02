# FLORIS Coupling Guide

## Overview

The standalone FLORIS coupling tool allows you to export wind speeds computed by the mass-consistent solver to FLORIS-compatible formats. **No FLORIS installation is required** for the export process itself.

## Architecture

```
massconsistent_amr (wind solver)
         ↓
    Solve wind field
         ↓
floris_coupling module (in this repo)
         ↓
    Extract & interpolate wind
         ↓
    Format for FLORIS
         ↓
CSV/JSON output
         ↓
(Optional) Use with FLORIS
```

## Quick Start

### 1. Basic Export (Standalone)

```python
from wind_solver import WindSolver
from floris_coupling import quick_export

# Solve wind field
wind = WindSolver("inputs.i")
wind.solve()

# Define turbine locations (x, y in meters)
turbines = [(100, 200), (300, 400), (500, 400)]

# Export wind speeds at turbine hubs
wind_data = quick_export(
    wind,
    turbines,
    hub_height=90.0,  # meters AGL
    output_file="floris_wind.csv",
    reference_speed=10.0  # optional: for speed-up ratios
)

wind.finalize()
```

### 2. Command-Line Tool

```bash
# Export to CSV
python3 tools/floris_export.py \
    --solver inputs.i \
    --turbines turbines.csv \
    --hub-height 90.0 \
    --output wind_data.csv

# With speed-up ratios
python3 tools/floris_export.py \
    --solver inputs.i \
    --turbines turbines.csv \
    --hub-height 90.0 \
    --reference-speed 10.0 \
    --output wind_data.csv

# Export to JSON
python3 tools/floris_export.py \
    --solver inputs.i \
    --turbines turbines.csv \
    --hub-height 90.0 \
    --output wind_data.json
```

### 3. Turbine Locations File Format

`turbines.csv`:
```
x,y
100.0,200.0
300.0,400.0
500.0,600.0
```

## Output Formats

### CSV Output

```
turbine_id,x,y,z_terrain,z_hub,u_ms,v_ms,speed_ms,direction_deg,speedup_ratio
0,100.0,200.0,50.0,140.0,5.2,1.3,5.33,345.2,1.05
1,300.0,400.0,60.0,150.0,4.8,1.1,4.93,346.1,0.98
```

Fields:
- `turbine_id`: Turbine index (0, 1, 2, ...)
- `x, y`: Turbine location (meters)
- `z_terrain`: Terrain elevation at turbine (meters MSL)
- `z_hub`: Hub height (absolute, meters MSL)
- `u_ms, v_ms`: Velocity components (m/s)
- `speed_ms`: Wind speed magnitude (m/s)
- `direction_deg`: Wind direction from north (0-360°, meteorological convention)
- `speedup_ratio`: Wind speed ratio to reference (optional)

### JSON Output

```json
{
  "solver_info": {
    "nx": 100,
    "ny": 100,
    "nz": 50,
    "dx_m": 10.0,
    "dy_m": 10.0,
    "dz_m": 10.0,
    "domain_x_range": [0.0, 990.0],
    "domain_y_range": [0.0, 990.0],
    "domain_z_range": [0.0, 490.0]
  },
  "extraction_info": {
    "hub_height_agl_m": 90.0,
    "reference_speed_ms": 10.0,
    "num_turbines": 5
  },
  "turbines": [
    {
      "id": 0,
      "location": {"x": 100.0, "y": 200.0},
      "terrain_elevation_m": 50.0,
      "hub_elevation_m": 140.0,
      "velocity": {
        "u_ms": 5.2,
        "v_ms": 1.3,
        "speed_ms": 5.33,
        "direction_deg": 345.2
      },
      "speedup_ratio": 1.05
    },
    ...
  ]
}
```

## Python API Reference

### FLORISWindMap Class

Main class for wind field extraction and export.

#### Initialization

```python
from floris_coupling import FLORISWindMap

wind_map = FLORISWindMap(wind_solver)
```

#### Methods

**get_wind_at_point(x, y, z)**

Get wind at a specific 3D point.

```python
wind = wind_map.get_wind_at_point(100.0, 200.0, 150.0)
# Returns: {
#     'u': 5.2,      # m/s
#     'v': 1.3,      # m/s
#     'speed': 5.33, # m/s
#     'direction': 345.2,  # degrees from north
#     'x': 100.0, 'y': 200.0, 'z': 150.0
# }
```

**get_wind_at_turbine(turbine_x, turbine_y, hub_height)**

Get wind at a turbine location (automatic terrain alignment).

```python
wind = wind_map.get_wind_at_turbine(100.0, 200.0, hub_height=90.0)
# Automatically adds terrain elevation to hub_height for z-coordinate
```

**get_wind_at_turbines(turbine_locations, hub_height)**

Get wind at multiple turbine locations.

```python
turbines = [(100, 200), (300, 400), (500, 600)]
winds = wind_map.get_wind_at_turbines(turbines, hub_height=90.0)
# Returns list of wind dicts, one per turbine
```

**export_to_csv(turbine_locations, hub_height, output_file, reference_speed=None)**

Export to CSV format.

```python
wind_map.export_to_csv(
    turbine_locations=[(100, 200), (300, 400)],
    hub_height=90.0,
    output_file="wind.csv",
    reference_speed=10.0  # optional
)
```

**export_to_json(turbine_locations, hub_height, output_file, reference_speed=None)**

Export to JSON format.

```python
wind_map.export_to_json(
    turbine_locations=[(100, 200), (300, 400)],
    hub_height=90.0,
    output_file="wind.json"
)
```

**export_to_dict(turbine_locations, hub_height, reference_speed=None)**

Export as Python dictionary (no file written).

```python
data = wind_map.export_to_dict(
    turbine_locations=[(100, 200), (300, 400)],
    hub_height=90.0
)
```

**get_speed_map_2d(height)**

Get 2D wind speed map at a specific height.

```python
speed_map, x_coords, y_coords = wind_map.get_speed_map_2d(height=90.0)
# speed_map shape: (ny, nx)
# Useful for visualization or speed-up analysis
```

### quick_export Function

Convenience function for simple one-call export.

```python
from floris_coupling import quick_export

wind_data = quick_export(
    wind_solver,
    turbine_locations=[(100, 200), (300, 400)],
    hub_height=90.0,
    output_file="floris_wind.csv",
    reference_speed=10.0  # optional
)
```

## Usage Examples

### Example 1: Basic Workflow

```python
from wind_solver import WindSolver
from floris_coupling import FLORISWindMap

# Initialize and solve
wind = WindSolver("inputs.i")
wind.solve()

# Create wind map
wind_map = FLORISWindMap(wind)

# Define turbine locations
turbines = [
    (100, 100),
    (200, 100),
    (300, 100),
    (150, 200),
    (250, 200),
]

# Export
wind_map.export_to_csv(
    turbine_locations=turbines,
    hub_height=90.0,
    output_file="farm_wind.csv"
)

wind.finalize()
```

### Example 2: With Speed-up Analysis

```python
from wind_solver import WindSolver
from floris_coupling import FLORISWindMap

wind = WindSolver("inputs.i")
wind.solve()

wind_map = FLORISWindMap(wind)

# Reference wind (e.g., from meteorological station)
ref_speed = 10.0  # m/s

# Export with speed-up ratios
turbines = [(100, 200), (300, 400)]
wind_map.export_to_csv(
    turbine_locations=turbines,
    hub_height=90.0,
    output_file="farm_wind.csv",
    reference_speed=ref_speed
)

wind.finalize()

# Analyze speed-up
import pandas as pd
df = pd.read_csv("farm_wind.csv")
print(f"Mean speed-up: {df['speedup_ratio'].mean():.3f}")
```

### Example 3: Query Arbitrary Points

```python
from wind_solver import WindSolver
from floris_coupling import FLORISWindMap

wind = WindSolver("inputs.i")
wind.solve()

wind_map = FLORISWindMap(wind)

# Query wind at specific 3D points
point1 = wind_map.get_wind_at_point(100, 200, 100)
point2 = wind_map.get_wind_at_point(300, 400, 150)

print(f"Speed at point 1: {point1['speed']:.2f} m/s")
print(f"Speed at point 2: {point2['speed']:.2f} m/s")

wind.finalize()
```

### Example 4: 2D Speed Map Visualization

```python
from wind_solver import WindSolver
from floris_coupling import FLORISWindMap
import matplotlib.pyplot as plt

wind = WindSolver("inputs.i")
wind.solve()

wind_map = FLORISWindMap(wind)

# Get 2D speed map
speed_map, x_coords, y_coords = wind_map.get_speed_map_2d(height=90.0)

# Visualize
plt.contourf(x_coords, y_coords, speed_map, levels=20, cmap='RdYlGn_r')
plt.colorbar(label='Wind Speed (m/s)')
plt.xlabel('X (m)')
plt.ylabel('Y (m)')
plt.title('Wind Speed Map at 90m AGL')
plt.savefig('speed_map.png', dpi=150)
plt.show()

wind.finalize()
```

### Example 5: Integration with FLORIS (Optional)

```python
from wind_solver import WindSolver
from floris_coupling import FLORISWindMap
import floris  # FLORIS must be installed separately

# Export wind with massconsistent_amr
wind = WindSolver("inputs.i")
wind.solve()

wind_map = FLORISWindMap(wind)

# Get wind data
turbines = [(100, 200), (300, 400)]
wind_data = wind_map.export_to_dict(turbines, hub_height=90.0)

# Use with FLORIS (example - check FLORIS docs for actual API)
farm = floris.Farm(configuration="floris_config.yaml")

for i, turbine in enumerate(wind_data['turbines'].values()):
    speed = turbine['speed']
    direction = turbine['direction']
    # Update FLORIS with wind data
    # (FLORIS API varies by version)
    farm.set_wind_at_turbine(i, speed, direction)

results = farm.run()
print(f"Total power: {results.power.sum():.2f} MW")

wind.finalize()
```

## Key Features

✅ **Standalone**: No FLORIS dependency for export  
✅ **Terrain-aware**: Automatically handles terrain elevation  
✅ **Accurate interpolation**: Tri-linear interpolation for smooth fields  
✅ **Multiple formats**: CSV, JSON, or Python dicts  
✅ **Speed-up analysis**: Compute speed-up ratios relative to reference wind  
✅ **Flexible**: Query arbitrary points, not just grid-aligned locations  
✅ **2D maps**: Generate speed maps for visualization  

## Limitations & Considerations

- **Interpolation**: Uses tri-linear interpolation. Extreme values outside the domain are clamped to domain bounds.
- **Terrain alignment**: Assumes 2D terrain model. Heights are automatically added to terrain elevation.
- **Reference wind**: Speed-up ratios are relative to specified reference speed. Choose reference carefully.
- **Grid resolution**: Wind data is limited by input grid resolution. Finer queries won't improve accuracy beyond grid scale.

## Troubleshooting

### "Could not import pyWindSolver" error

Make sure to set PYTHONPATH:
```bash
export PYTHONPATH=/path/to/massconsistent_amr/build/python:$PYTHONPATH
```

### Point outside domain error

Check that turbine locations are within the solved domain:
```python
print(f"Domain X: [{wind_map.xmin}, {wind_map.xmin + (wind_map.nx-1)*wind_map.dx}]")
print(f"Domain Y: [{wind_map.ymin}, {wind_map.ymin + (wind_map.ny-1)*wind_map.dy}]")
```

### Unexpected speed values

- Check that wind field was solved successfully (check residual)
- Verify terrain and wind initialization parameters
- Check hub height is within domain height bounds

## Citation & References

If you use this tool in research, please cite:
- massconsistent_amr: [GitHub repository link]
- FLORIS: NREL publication details

## Contact & Support

For issues or questions:
- massconsistent_amr: GitHub Issues
- FLORIS: FLORIS GitHub repository
- Integration questions: Open GitHub Issue on massconsistent_amr
