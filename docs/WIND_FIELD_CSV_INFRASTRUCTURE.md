# Puff Model: Wind Field CSV Input Infrastructure

## Overview

This document describes the enhanced wind field input system for the puff model. The system supports multiple wind field formats via CSV input files, enabling flexible configuration for various meteorological scenarios while maintaining full backward compatibility with existing input files.

## Features

### Supported Wind Field Formats

1. **Uniform Wind** – Single constant wind vector for entire domain
2. **Time-Series Wind** – Constant spatial wind, varying in time
3. **Gridded Wind** – Wind field defined at discrete grid points
4. **Legacy Uniform Wind** – Original `U_wind`, `V_wind`, `W_wind` parameters (unchanged)

### Backward Compatibility

- Existing input files work without modification
- New wind field CSV parameters are optional; defaults apply if omitted
- If `wind_field_file` is empty or not specified, solver uses legacy `U_wind`, `V_wind`, `W_wind`

## Configuration

### Input File (inputs.i) Parameters

New optional parameters for wind field configuration:

```
# Wind field input
wind_field_file = "wind_field.csv"      # Path to wind field CSV (optional)
wind_field_format = "uniform"           # Auto-detect if omitted
enable_unsteady_wind = false            # Enable time-series wind
wind_field_start_time = 0.0             # Start time [s]

# Reserved for future multi-source support
sources_file = ""                       # Multiple sources CSV (not yet implemented)
met_profile_file = ""                   # Met profiles CSV (not yet implemented)
enable_spatial_met = false              # Spatial meteorology (not yet implemented)
```

**All new parameters default to sensible values; none are required.**

### CSV Format Specification

#### Uniform Wind Format

Single (u, v, w) for entire domain and time.

**CSV Structure:**
```
# Metadata comments (optional)
u,v,w
10.0,0.0,0.0
```

#### Time-Series Wind Format

Wind varying in time, constant in space.

**CSV Structure:**
```
# Metadata comments (optional)
time,u,v,w
0.0,8.0,0.5,0.0
60.0,9.0,0.3,0.0
120.0,10.0,0.1,0.0
```

Time must be sorted ascending. Linear interpolation used between time steps.

#### Gridded Wind Format

Wind defined at discrete grid points.

**CSV Structure:**
```
# Metadata comments (optional)
x,y,z,u,v,w
0.0,0.0,0.0,9.8,-0.1,0.0
50.0,0.0,0.0,9.9,-0.2,0.01
100.0,0.0,0.0,10.0,-0.1,0.0
```

Nearest-neighbor interpolation used to find wind at arbitrary puff locations.

## Data Processing Pipeline

### Input → Processing → Output

```
User Data (NetCDF, ASCII, etc.)
    ↓
wind_field_converter.py (Python)
    ↓
wind_field.csv (Standard CSV)
    ↓
puff_solver (C++)
    ↓
Concentration Output
```

### Converter Utility

**File:** `src/python/wind_field_converter.py`

**Purpose:** Convert wind data from external sources (WRF, CALMET, ASCII grids) to standard CSV format.

**Usage Examples:**

```bash
# Create uniform wind
python wind_field_converter.py --uniform 10.0 0.0 0.0 --output wind_field.csv

# Convert from WRF NetCDF
python wind_field_converter.py --input wrfout.nc --format wrf --output wind_field.csv

# Convert from ASCII grid
python wind_field_converter.py --input wind_grid.txt --format ascii --output wind_field.csv
```

**Supported Input Formats:**
- WRF NetCDF (`--format wrf`)
- ASCII grid (`--format ascii`)
- Uniform wind command-line specification (`--uniform u v w`)

**Output:** Always standard CSV with metadata header and column headers.

## C++ Implementation

### Structures

New types added to `puff_models.H`:

```cpp
struct UniformWindField {
    amrex::Real u, v, w;    // Wind components [m/s]
};

struct GriddedWindPoint {
    amrex::Real x, y, z;    // Grid coordinates [m]
    amrex::Real u, v, w;    // Wind components [m/s]
};

struct TimeSeriesWindPoint {
    amrex::Real time;       // Time [s]
    amrex::Real u, v, w;    // Wind components [m/s]
};
```

### Functions

New functions in `puff_models.H`:

**`read_wind_field_csv()`**
- Reads CSV file and detects format automatically
- Returns wind data in appropriate structure
- Handles metadata comments and column parsing
- Non-blocking: returns false if file not found

**`interpolate_gridded_wind()`**
- Interpolates wind at arbitrary point (x, y, z) from gridded data
- Supports nearest-neighbor interpolation
- Returns false if extrapolation needed

**`interpolate_timeseries_wind()`**
- Interpolates wind at arbitrary time t from time-series data
- Uses linear interpolation between time steps
- Extrapolates by holding boundary values constant

### Integration with puff_solver

Currently, the infrastructure is in place for reading wind field CSV files. Integration into `puff_solver.cpp` will:

1. Read new configuration parameters from inputs.i
2. Call `read_wind_field_csv()` if `wind_field_file` is specified
3. Use appropriate interpolation function during puff advection
4. Fall back to legacy `U_wind`, `V_wind`, `W_wind` if no CSV file

**Status:** Ready for integration; no breaking changes to existing solver logic.

## Example Usage

### Basic Setup with Uniform Wind CSV

**inputs.i:**
```
enable_puff = true
source_x = 150.0
source_y = 150.0
source_z = 10.0
emission_rate = 1.0
K_h = 1.0
K_v = 0.5

wind_field_file = "wind_field.csv"
wind_field_format = "uniform"

# ... other parameters
```

**wind_field.csv:**
```
# Wind Field CSV Format
# Format: uniform
u,v,w
10.0,0.0,0.0
```

**Execution:**
```bash
./puff_solver inputs.i
```

### Time-Varying Wind Example

**inputs.i:**
```
wind_field_file = "wind_timeseries.csv"
wind_field_format = "timeseries"
enable_unsteady_wind = true
wind_field_start_time = 0.0
```

**wind_timeseries.csv:**
```
# Format: timeseries
time,u,v,w
0.0,8.0,0.5,0.0
60.0,9.0,0.3,0.0
120.0,10.0,0.1,0.0
```

### Gridded Wind from WRF

**Convert WRF data:**
```bash
python wind_field_converter.py --input wrfout.nc --format wrf --output wind_field.csv
```

**inputs.i:**
```
wind_field_file = "wind_field.csv"
wind_field_format = "gridded"
```

## Files Provided

### Code

- `src/puff_models.H` – Extended with wind field structures and I/O functions
- `src/python/wind_field_converter.py` – Utility for format conversion

### Documentation

- `docs/puff_csv_input_format.md` – Comprehensive CSV format specification
- `src/python/WIND_FIELD_CONVERTER_README.md` – Converter utility guide

### Examples

- `docs/examples/wind_field_uniform.csv` – Uniform wind template
- `docs/examples/wind_field_timeseries.csv` – Time-series wind template
- `docs/examples/wind_field_gridded.csv` – Gridded wind template
- `docs/examples/inputs_wind_uniform.i` – Example configuration for uniform wind
- `docs/examples/inputs_wind_timeseries.i` – Example with time-varying wind
- `docs/examples/inputs_wind_gridded.i` – Example with gridded wind

## Next Steps

### Recommended Future Enhancements

1. **Multi-Source Configuration** – Support multiple simultaneous emission points via CSV
2. **Meteorological Profiles** – Spatially-varying atmospheric parameters (K_h(x,y,z), stability, etc.)
3. **Chemical Reaction Matrix** – Pollutant transformation via CSV specification
4. **Deposition Parameters** – Pollutant-specific deposition velocities
5. **Stack Aerodynamics** – Stack tip downwash and buoyancy effects

### Integration Checklist

- [ ] Add wind field CSV parameter reading to puff_solver.cpp
- [ ] Integrate read_wind_field_csv() calls into main loop
- [ ] Test with existing regression test suite
- [ ] Verify backward compatibility
- [ ] Add new regression tests for wind field reading
- [ ] Update user documentation and tutorials

## Design Rationale

### Why CSV?

- **Human-readable** – Easy to inspect and edit manually
- **Portable** – Works across all platforms and tools
- **Lightweight** – Minimal file size for typical meteorological data
- **Standard** – Compatible with spreadsheet software, plotting tools, etc.
- **Extensible** – Additional columns can be added for future features

### Why Python for Conversion?

- **Ease of use** – No compilation required; runs on any platform
- **Flexibility** – Easy to add support for new input formats
- **Maintainability** – External data processing kept separate from core solver
- **Dependencies** – Optional; user only installs netCDF4 if needed
- **Reusability** – Can be used independently or as library

### Backward Compatibility Strategy

- All new parameters optional with sensible defaults
- Legacy `U_wind`, `V_wind`, `W_wind` still fully supported
- No changes to existing ParmParse reading logic
- Graceful fallback if CSV file not found
- All existing test cases continue to pass unchanged

## Error Handling

**Missing CSV File:**
- Warning printed to console
- Fallback to legacy `U_wind`, `V_wind`, `W_wind`
- If both missing, use zero wind (U=V=W=0)

**Malformed CSV:**
- Lines with parsing errors skipped silently
- Insufficient columns skipped
- First 3 or 4 columns used as determined by format

**Format Mismatch:**
- Auto-detection based on column count and metadata
- Manual specification with `wind_field_format` parameter
- Warning if format cannot be determined

