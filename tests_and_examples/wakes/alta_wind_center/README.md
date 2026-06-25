# Alta Wind Energy Center (AWEC) Simulation Case

This directory contains a complete wind resource and analytical wake simulation case for the **Alta Wind Energy Center** (also known as the Mojave Wind Farm) in Tehachapi Pass, Kern County, California.

## Case Overview

The Alta Wind Energy Center is one of the largest onshore wind farms in the world. This case replicates a realistic subset of 39 turbines arranged along three major North-South running mountain ridges of the Tehachapi Pass topography.

- **Terrain**: Realistically modeled ridges and valleys ranging from 750m to 1200m in altitude, channeling wind flow.
- **Turbines**: 39 turbines positioned at ridge peaks to maximize inflow speeds, with hub heights of 80m and rotor diameters of 80m.
- **Wind Profile**: A standard power-law profile representing 8 m/s wind from the west ($U_{ref} = 8.0$ m/s, $z_{ref} = 80.0$ m, $\alpha = 0.15$) under neutral atmospheric conditions.
- **Wake Deficits**: Solved using the Bastankhah-Gaussian analytical wake deficit model with quadratic wake superposition.

## Contents

1. **`test_alta_wind_center.py`**: The main simulation and test runner. Initializes the `WindSolver` with the power-law profile and ridge terrain, solves the flow field with analytical wakes, extracts wind velocity at 80m above terrain (hub height), saves a 2D wake contour map (`alta_wake_80m.png`), and records turbine performance to a CSV.
2. **`plot_power.py`**: A dedicated visualization script that loads the simulated turbine performance data and generates:
   - A performance chart (`alta_power_bars.png`) comparing inflow speeds and power output per turbine, highlighting ridge shading.
   - A spatial distribution map (`alta_power_spatial.png`) displaying the geographical layout with marker sizes and colors denoting generated power.
3. **`nrel_5mw.csv`**: Reference power curve and thrust coefficient profile for a typical 2MW-5MW wind turbine.
4. **`inputs.i`**: Solver configuration file.
5. **`turbines.csv`**: Wind turbine coordinate and specification registry.
6. **`terrain.csv`**: Discretized 3D surface elevation grid representing Tehachapi Pass ridges.

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

- **West Ridge (Upstream)**: Turbines face unobstructed 8 m/s westerly winds and produce maximum power output (~3395 kW).
- **Central Ridge (Downstream)**: Experience wake shadowing effects from the West Ridge; inflow speeds are reduced (~7.75 m/s), leading to lower power output (~3200 kW).
- **East Ridge (Deep Shadow)**: Experience cumulative wake deficits from both preceding ridges, resulting in the lowest power output (~3175 kW).
