# Wind Field Converter Utility

Converts wind field data from various sources into standardized CSV formats compatible with the puff model.

## Installation

The utility requires Python 3.6+. Optional dependencies can be installed as needed:

```bash
# Basic usage (CSV, uniform wind)
python wind_field_converter.py --uniform 10.0 0.0 0.0 --output wind_field.csv

# WRF NetCDF support (requires netCDF4)
pip install netCDF4
python wind_field_converter.py --input wrfout.nc --format wrf --output wind_field.csv

# NumPy support (for advanced operations)
pip install numpy
```

## Usage

### Create Uniform Wind Field

Simplest case: single constant wind vector for entire domain.

```bash
python wind_field_converter.py \
  --uniform 10.0 0.0 0.0 \
  --output wind_field.csv \
  --description "10 m/s easterly wind"
```

### Convert from WRF NetCDF

Extract wind field from WRF model output.

```bash
python wind_field_converter.py \
  --input wrfout_d01_2023-06-15_00_00_00 \
  --format wrf \
  --output wind_field.csv \
  --description "WRF hourly wind"
```

Supports:
- 10m wind fields (`U10`, `V10`)
- Full-level fields (`U`, `V`, `W`)
- Multiple time steps (extracts first time step by default)

### Convert from ASCII Grid

Generic ASCII grid with columns: x, y, z, u, v, w

```bash
python wind_field_converter.py \
  --input wind_grid.txt \
  --format ascii \
  --delimiter space \
  --output wind_field.csv
```

Supported delimiters: `space`, `comma`, `tab`

## Output Format Detection

The converter automatically generates appropriate CSV format:

- **1 data row, 3 columns** → uniform format
- **N rows, 6 columns** → gridded format
- **N rows, 4 columns** → time-series format

## Metadata

Output CSV files include metadata header:

```csv
# Wind Field CSV Format
# Description: [user description]
# Format: [uniform|gridded|timeseries]
# [Additional metadata]

[column headers]
[data rows]
```

## Integration with Puff Model

Use output CSV file in puff model input:

```
wind_field_file = "wind_field.csv"
wind_field_format = "uniform"    # auto-detected if not specified
```

## Examples

See `docs/examples/` for complete examples:
- `wind_field_uniform.csv` – Single uniform wind
- `wind_field_timeseries.csv` – Time-varying wind
- `wind_field_gridded.csv` – Gridded spatial wind field
- `inputs_wind_uniform.i` – Example puff_solver configuration
- `inputs_wind_timeseries.i` – Example with unsteady wind
- `inputs_wind_gridded.i` – Example with gridded wind

## Error Handling

- Missing input file → error with exit code 1
- Unsupported format → error with exit code 1
- Malformed CSV → warning; skipped lines
- Missing netCDF4 package → error message with installation hint

## Performance Notes

- **Uniform wind:** < 1 KB file, instant processing
- **Gridded wind:** File size scales with grid points (e.g., 10×10×5 = ~3 KB)
- **Time-series wind:** File size scales with time steps (e.g., 100 steps = ~2 KB)

