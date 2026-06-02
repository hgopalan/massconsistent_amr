# Tools Directory

This directory contains utility scripts for the massconsistent_amr wind solver.

## Available Tools

### floris_export.py

Standalone command-line tool for exporting wind field data to FLORIS-compatible formats.

**Features:**
- No FLORIS installation required (standalone exporter)
- Export wind speeds at arbitrary turbine locations
- Support for CSV and JSON output formats
- Optional speed-up ratio calculation relative to reference wind
- Automatic terrain-aware wind extraction

**Requirements:**
- Python 3.6+
- numpy
- massconsistent_amr with Python bindings (`-DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON`)

**Usage:**

```bash
# Export to CSV
python3 floris_export.py --solver inputs.i --turbines turbines.csv \
    --hub-height 90.0 --output wind_data.csv

# Export with speed-up ratios
python3 floris_export.py --solver inputs.i --turbines turbines.csv \
    --hub-height 90.0 --reference-speed 10.0 --output wind_data.csv

# Export to JSON
python3 floris_export.py --solver inputs.i --turbines turbines.csv \
    --output wind_data.json

# Verbose output
python3 floris_export.py --solver inputs.i --turbines turbines.csv \
    --output wind_data.csv --verbose
```

**Input Files:**

Turbine locations CSV (`turbines.csv`):
```
x,y
100.0,200.0
300.0,400.0
500.0,600.0
```

**Output:**

CSV format (`wind_data.csv`):
```
turbine_id,x,y,z_terrain,z_hub,u_ms,v_ms,speed_ms,direction_deg,speedup_ratio
0,100.0,200.0,50.0,140.0,5.2,1.3,5.33,345.2,1.05
1,300.0,400.0,60.0,150.0,4.8,1.1,4.93,346.1,0.98
```

For full documentation, see `../docs/FLORIS_COUPLING.md`

### hrrr_to_surface_data.py

Extract surface parameters from HRRR GRIB2 files for wind solver initialization.

**Features:**
- Download HRRR data from meteorological servers
- Extract surface roughness, friction velocity, and 10m winds
- Convert to surface_data.csv format for wind solver

**Usage:**

```bash
python3 hrrr_to_surface_data.py --grib hrrr.grib2 \
    --output surface_data.csv --bbox xmin xmax ymin ymax
```

### farsite_weather_reader.py

Read FARSITE weather station data and prepare for wind coupling.

## Setting Up Python Environment

### With Conda

```bash
conda create -n windtools python=3.9 numpy scipy pandas matplotlib
conda activate windtools
```

### With venv

```bash
python3 -m venv venv
source venv/bin/activate
pip install numpy scipy pandas matplotlib
```

### Set PYTHONPATH

After building massconsistent_amr with Python bindings:

```bash
export PYTHONPATH=/path/to/massconsistent_amr/build/python:$PYTHONPATH
```

## Examples

Run example usage:

```bash
PYTHONPATH=../build/python python3 ../src/python/example_floris_export.py
```

## Documentation

- **FLORIS Coupling Guide**: `../docs/FLORIS_COUPLING.md`
- **Wind Solver API**: `../docs/` (see main documentation)

## Integration with FLORIS

To use the exported wind data with FLORIS:

1. Export wind data using `floris_export.py`
2. Install FLORIS: `pip install floris`
3. Load wind data in your FLORIS scripts
4. Run wind farm simulation

Example FLORIS integration (see `../src/python/example_floris_export.py` for details):

```python
from wind_solver import WindSolver
from floris_coupling import FLORISWindMap
import floris

# Export wind
wind = WindSolver("inputs.i")
wind.solve()
wind_map = FLORISWindMap(wind)

# Get wind data
turbines = [(100, 200), (300, 400)]
wind_data = wind_map.export_to_dict(turbines, hub_height=90.0)

# Use with FLORIS...
farm = floris.Farm(configuration="config.yaml")
# (See FLORIS documentation for API usage)

wind.finalize()
```

## Troubleshooting

### ImportError: Could not import pyWindSolver

Make sure:
1. massconsistent_amr is built with Python bindings
2. PYTHONPATH is set correctly
3. Python version matches build (typically 3.8+)

```bash
# Build with Python bindings
cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build

# Set PYTHONPATH
export PYTHONPATH=/path/to/build/python:$PYTHONPATH
```

### Wind field not solved error

Make sure to call `wind.solve()` before exporting:

```python
wind = WindSolver("inputs.i")
wind.solve()  # Required!
export_wind(wind, ...)
```

### Point outside domain error

Verify turbine locations are within domain bounds:

```python
print(f"X range: [{wind.xmin}, {wind.xmin + (wind.nx-1)*wind.dx}]")
print(f"Y range: [{wind.ymin}, {wind.ymin + (wind.ny-1)*wind.dy}]")
```

## Contributing

To add new tools:
1. Place script in this directory
2. Add documentation header with usage examples
3. Include error handling and help messages
4. Test with sample data
5. Document in this README

## License

Same as massconsistent_amr (see LICENSE file in root directory)
