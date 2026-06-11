# Migration Guide: Transitioning to Wind Field CSV

This guide provides step-by-step instructions for users to migrate from legacy uniform wind parameters to the new CSV-based wind field system.

## Before: Legacy Approach

**inputs.i (old way):**
```
enable_puff = true
source_x = 150.0
source_y = 150.0
source_z = 10.0
emission_rate = 1.0

# Wind specified directly in inputs.i
U_wind = 10.0
V_wind = 0.0
W_wind = 0.0

# ... other parameters
```

## After: CSV-Based Approach

### Step 1: Create Wind Field CSV

Create `wind_field.csv`:
```
# Wind Field CSV Format
# Description: 10 m/s easterly wind
# Format: uniform

u,v,w
10.0,0.0,0.0
```

Alternatively, use Python converter:
```bash
python wind_field_converter.py --uniform 10.0 0.0 0.0 --output wind_field.csv
```

### Step 2: Update inputs.i

**inputs.i (new way):**
```
enable_puff = true
source_x = 150.0
source_y = 150.0
source_z = 10.0
emission_rate = 1.0

# Wind now specified in CSV file
wind_field_file = "wind_field.csv"
wind_field_format = "uniform"

# ... other parameters
```

Remove the legacy `U_wind`, `V_wind`, `W_wind` lines (optional; if present, CSV takes precedence).

### Step 3: Run Solver

```bash
./puff_solver inputs.i
```

The solver will read wind from `wind_field.csv` instead of inputs.i parameters.

## Transition Scenarios

### Scenario 1: Simple Uniform Wind (Most Common)

**Legacy (old):**
```
U_wind = 5.0
V_wind = 2.0
W_wind = 0.0
```

**Modern (new):**

1. Create `wind_field.csv`:
   ```
   u,v,w
   5.0,2.0,0.0
   ```

2. Update `inputs.i`:
   ```
   wind_field_file = "wind_field.csv"
   wind_field_format = "uniform"
   ```

**Benefits:**
- Wind parameters now in dedicated file, easier to manage
- Can reuse same wind file across multiple simulations
- Easier to document and track wind field conditions

### Scenario 2: Wind Varying with Time

**Cannot be done with legacy approach.**

**Modern approach:**

1. Create `wind_timeseries.csv`:
   ```
   # Format: timeseries
   time,u,v,w
   0.0,3.0,0.0,0.0
   60.0,5.0,1.0,0.0
   120.0,8.0,2.0,0.0
   180.0,10.0,1.0,0.0
   ```

2. Update `inputs.i`:
   ```
   wind_field_file = "wind_timeseries.csv"
   wind_field_format = "timeseries"
   enable_unsteady_wind = true
   ```

3. Run solver with time steps covering 0-180 seconds:
   ```
   dt_puff = 1.0
   n_steps_puff = 200
   ```

### Scenario 3: Gridded Wind from WRF Model

**Cannot be done with legacy approach.**

**Modern approach:**

1. Convert WRF NetCDF to CSV:
   ```bash
   python wind_field_converter.py --input wrfout.nc --format wrf --output wind_field.csv
   ```

2. Update `inputs.i`:
   ```
   wind_field_file = "wind_field.csv"
   wind_field_format = "gridded"
   ```

3. Run solver:
   ```bash
   ./puff_solver inputs.i
   ```

The solver will interpolate WRF wind to each puff location automatically.

## Backward Compatibility

**Old input files still work unchanged:**

If your current `inputs.i` uses legacy parameters:
```
U_wind = 10.0
V_wind = 0.0
W_wind = 0.0
```

The solver will continue to work with these parameters. No migration is required unless you want to use advanced features like time-varying or gridded wind.

**Migration is optional.** Start using CSV format when:
- You need time-varying wind fields
- You have gridded meteorological data
- You want to manage wind data separately from solver configuration
- You want to reuse wind fields across multiple simulations

## Common Patterns

### Pattern 1: Batch Processing Multiple Simulations with Same Wind

Create single `wind_field.csv`, reuse with multiple `inputs_*.i` files:

```
wind_field.csv
inputs_scenario1.i  (wind_field_file = "wind_field.csv")
inputs_scenario2.i  (wind_field_file = "wind_field.csv")
inputs_scenario3.i  (wind_field_file = "wind_field.csv")
```

### Pattern 2: Parametric Study with Different Winds

Create multiple wind CSV files:

```
wind_5ms.csv        (5 m/s wind)
wind_10ms.csv       (10 m/s wind)
wind_15ms.csv       (15 m/s wind)
inputs_base.i       (shared configuration)
```

Create wrapper script to run solver with each wind:
```bash
for wind_file in wind_*.csv; do
  cp inputs_base.i temp_inputs.i
  sed -i "s/wind_field_file = .*/wind_field_file = \"$wind_file\"/" temp_inputs.i
  ./puff_solver temp_inputs.i
done
```

### Pattern 3: Data-Driven Simulations

Convert external data to CSV once, then run multiple scenarios:

```bash
# Convert WRF data once
python wind_field_converter.py --input wrfout.nc --format wrf --output wind_wrf.csv

# Run multiple scenarios with same meteorological data
./puff_solver inputs_scenario1.i  # uses wind_wrf.csv
./puff_solver inputs_scenario2.i  # uses wind_wrf.csv
./puff_solver inputs_scenario3.i  # uses wind_wrf.csv
```

## Format Conversion Recipes

### From WRF NetCDF

```bash
python wind_field_converter.py \
  --input wrfout_d01_2023-06-15_00:00:00 \
  --format wrf \
  --output wind_wrf.csv \
  --description "WRF hourly output from 2023-06-15"
```

### From ASCII Grid (space-delimited)

```bash
python wind_field_converter.py \
  --input wind_grid.txt \
  --format ascii \
  --delimiter space \
  --output wind_ascii.csv
```

### From ASCII Grid (comma-delimited)

```bash
python wind_field_converter.py \
  --input wind_data.csv \
  --format ascii \
  --delimiter comma \
  --output wind_converted.csv
```

### Create Uniform Wind Programmatically

```bash
python wind_field_converter.py \
  --uniform 10.0 0.0 0.0 \
  --output wind_uniform.csv \
  --description "Constant 10 m/s easterly wind for testing"
```

## Troubleshooting

### Issue: Solver Ignores `wind_field_file` Parameter

**Check:**
1. File exists in correct path:
   ```bash
   ls -la wind_field.csv
   ```

2. Path is correct in inputs.i:
   ```bash
   grep wind_field_file inputs.i
   ```

3. CSV file is readable:
   ```bash
   head wind_field.csv
   ```

**If file not found:** Solver prints warning and falls back to legacy `U_wind`, `V_wind`, `W_wind`.

### Issue: Wind Field Not Applied to Puffs

**Check:**
1. `wind_field_file` parameter present in inputs.i
2. CSV file path relative to current directory where solver runs
3. Format specified or auto-detected correctly:
   ```bash
   head -5 wind_field.csv  # Check metadata and format
   ```

### Issue: Time-Series Wind Not Varying

**Check:**
1. `enable_unsteady_wind = true` in inputs.i
2. `wind_field_format = "timeseries"` set correctly
3. Simulation time steps cover range of wind time series:
   ```
   n_steps_puff * dt_puff >= wind_timeseries_max_time
   ```

## Quick Reference

| Scenario | Method | Requires CSV? | Complexity |
|----------|--------|--------------|-----------|
| Constant uniform wind | Legacy OR CSV | No | Low |
| Time-varying wind | CSV only | Yes | Medium |
| Gridded spatial wind | CSV only | Yes | Medium |
| WRF model data | CSV conversion | Yes | Medium |
| Multi-scale simulations | Multiple CSV | Yes | High |

## See Also

- `docs/puff_csv_input_format.md` – Complete CSV format specification
- `src/python/WIND_FIELD_CONVERTER_README.md` – Converter utility guide
- `docs/examples/` – Example configurations and CSV files

