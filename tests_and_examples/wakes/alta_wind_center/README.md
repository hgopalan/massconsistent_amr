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

## Turbine Location Data

Turbine coordinates are based on:
- **USGS Topographic Mapping**: Three distinct N-S running ridges identified in Tehachapi Pass
- **Geographical Analysis**: Ridge positions at approximately -118.34°W, -118.32°W, and -118.30°W longitude
- **Wind Farm Siting Patterns**: Turbines positioned to follow ridge peaks and maximize wind exposure
- **UTM Zone 11N Projection**: All coordinates converted to UTM for accurate spatial analysis and modeling

The turbine distribution (200 per ridge, 20 rows × 10 columns) represents a realistic density for utility-scale wind farm layouts in complex terrain.

## Running the Case

To run the simulation and generate all outputs, execute:

```bash
# 1. Run the simulation and verification test
export PYTHONPATH=/path/to/massconsistent_amr/build/python:$PYTHONPATH
python3 test_alta_wind_center.py

# 2. Run the power visualization plots
python3 plot_power.py
```

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
