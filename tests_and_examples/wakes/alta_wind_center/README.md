# Alta Wind Energy Center (AWEC) Simulation Case

This directory contains a complete wind resource and analytical wake simulation case for the **Alta Wind Energy Center** (also known as the Mojave Wind Farm) in Tehachapi Pass, Kern County, California.

## Case Overview

The Alta Wind Energy Center is one of the largest onshore wind farms in the world. This case simulates 600 turbines arranged along three major North-South running mountain ridges of the Tehachapi Pass topography, based on realistic geographic coordinates from USGS topographic mapping.

- **Terrain**: Realistically modeled ridges and valleys ranging from 750m to 1200m in altitude, channeling wind flow.
- **Turbines**: 600 turbines (200 per ridge) positioned at ridge peaks to maximize inflow speeds:
  - **West Ridge (lon = -118.34)**: 200 turbines, windward exposure, minimal wake effects
  - **Center Ridge (lon = -118.32)**: 200 turbines, intermediate position, moderate wake deficits
  - **East Ridge (lon = -118.30)**: 200 turbines, lee side, strongest cumulative wake deficits
- **Hub Heights**: 80m, **Rotor Diameters**: 80m for all turbines.
- **Wind Profile**: A standard power-law profile representing 8 m/s wind from the west ($U_{ref} = 8.0$ m/s, $z_{ref} = 80.0$ m, $\alpha = 0.15$) under neutral atmospheric conditions.
- **Wake Deficits**: Solved using the Bastankhah-Gaussian analytical wake deficit model with quadratic wake superposition.
- **Coordinates**: Turbines positioned in UTM Zone 11N (California), based on geographical analysis of Tehachapi Pass ridge system.
- **Geographic Bounds**: 
  - Latitude: 35.025°N to 35.045°N
  - Longitude: -118.34°W to -118.30°W (Kern County, CA)

## Contents

1. **`test_alta_wind_center.py`**: The main simulation and test runner. Initializes the `WindSolver` with the power-law profile and ridge terrain, solves the flow field with analytical wakes for 600 turbines, extracts wind velocity at 80m above terrain (hub height), saves a 2D wake contour map (`alta_wake_80m.png`), and records turbine performance to a CSV.
2. **`plot_power.py`**: A dedicated visualization script that loads the simulated turbine performance data and generates:
   - A performance chart (`alta_power_bars.png`) comparing inflow speeds and power output per turbine, organized by ridge.
   - A spatial distribution map (`alta_power_spatial.png`) displaying the geographical layout in UTM coordinates with marker sizes and colors denoting generated power.
3. **`nrel_5mw.csv`**: Reference power curve and thrust coefficient profile for a typical 2MW-5MW wind turbine.
4. **`inputs.i`**: Solver configuration file with MLMG smoothing parameters tuned for aspect ratio of 8.0.
5. **`turbines.csv`**: Wind turbine coordinate and specification registry (600 turbines).
6. **`terrain.csv`**: Discretized 3D surface elevation grid representing Tehachapi Pass ridges.
7. **`TURBINE_LOCATIONS.md`**: Detailed documentation of coordinate system, geographic references, and turbine location data sources.

## Turbine Location Data

This simulation case uses **exact turbine coordinates from the USGS Wind Turbine Database (USWTB)**:

- **Data Source**: USGS Wind Turbine Database
  - Website: https://energy.usgs.gov/uswtdb/
  - Download: https://energy.usgs.gov/uswtdb/assets/data/uswtdbCSV.zip
  
- **Turbine Specification**:
  - Filter for "Alta Wind Energy Center" projects
  - Extract longitude, latitude, hub height, rotor diameter
  - Convert from WGS84 (geographic) to UTM Zone 11N (projected)

- **Coordinate Processing**:
  - Geographic coordinates: WGS84 datum (EPSG:4326)
  - Projected coordinates: UTM Zone 11N (EPSG:32611)
  - Automatic domain sizing based on actual turbine bounds
  - 600m buffer applied around turbine bounding box

### Obtaining the Data

Use the provided tool to download and convert USWTB coordinates:

```bash
# Extract Alta Wind turbines from USWTB database
python3 tools/data_ingestion/fetch_uswtb_turbines.py \
  --output tests_and_examples/wakes/alta_wind_center/turbines_uswtb.csv \
  --project "Alta Wind"
```

Or manually:
1. Visit https://energy.usgs.gov/uswtdb/
2. Select "Alta Wind" in the project filter
3. Download the CSV with turbine coordinates
4. Save as `turbines_uswtb.csv` in this directory

**See `TURBINE_LOCATIONS.md` for detailed coordinate system documentation.**

## Running the Case

To run the simulation with actual USWTB turbine coordinates:

### Step 1: Download USWTB Turbine Data

Download the USGS Wind Turbine Database:
```bash
# Download from USGS
wget https://energy.usgs.gov/uswtdb/assets/data/uswtdbCSV.zip

# Or use the fetch tool to get filtered data
python3 tools/data_ingestion/fetch_uswtb_turbines.py \
  --output tests_and_examples/wakes/alta_wind_center/turbines_uswtb.csv \
  --project "Alta Wind"
```

### Step 2: Run the Simulation

```bash
export PYTHONPATH=/path/to/massconsistent_amr/build/python:$PYTHONPATH
cd tests_and_examples/wakes/alta_wind_center
python3 test_alta_wind_center.py
python3 plot_power.py
```

The test will automatically:
1. Load turbine coordinates from `turbines_uswtb.csv` (real USWTB data)
2. Convert coordinates to UTM Zone 11N projection
3. Generate terrain based on actual coordinate bounds
4. Solve the wind field with analytical wake models
5. Output power results and visualization

## Expected Results

With 600 turbines distributed across three ridges:

- **West Ridge (Upstream)**: 200 turbines face unobstructed westerly winds with minimal wake effects. Average inflow speeds near 8 m/s with maximum power output.
- **Center Ridge (Intermediate)**: 200 turbines experience wake deficits from upwind West Ridge. Average inflow speeds reduced (~7.5 m/s) due to wake shadowing effects.
- **East Ridge (Lee Side)**: 200 turbines experience cumulative wake deficits from both West and Center ridges. Average inflow speeds further reduced (~7.5 m/s) with the lowest power output across the array.

The simulation demonstrates:
- Clear wake deficit propagation from windward (West) to leeward (East) ridges
- Spatial wind speed distribution showing wind speed recovery between ridges due to wake expansion
- Power output variation correlating directly with inflow speed reduction along the wind direction

## Notes on Solver Configuration

The simulation uses MLMG (Multi-Level Multi-Grid) solver with tuned smoothing parameters:
- `mlmg.num_pre_smooth = 16`
- `mlmg.num_post_smooth = 16`

These settings ensure convergence without divergence for the high aspect ratio of 8.0 (dx = dy = 120m, dz = 15m) used in this case.
