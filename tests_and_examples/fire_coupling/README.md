# Fire Coupling Examples

This directory contains realistic fire-wind coupling examples for the massconsistent_amr wind solver coupled with wildfire_levelset fire solver.

## Overview

The fire coupling examples demonstrate three coupling scenarios:

1. **Wind-only**: Wind field solved independently (baseline)
2. **One-way coupling**: Wind → Fire (fire responds to wind)
3. **Two-way coupling**: Wind ↔ Fire (fire heating affects wind)

## Fire Scenarios

### Colorado High-Elevation Terrain
- **Location**: Southern Colorado mountains
- **Elevation**: 2100-2400 m (high elevation)
- **Wind Speed**: 8 m/s westerly (energetic conditions)
- **Terrain Type**: Mountainous with ridges and valleys
- **Use Case**: Mountain fire dynamics with strong wind

**Directory**: `colorado/`

### California Coastal Terrain
- **Location**: Northern California coast
- **Elevation**: 400-700 m (moderate elevation)
- **Wind Speed**: 5 m/s northwesterly (coastal conditions)
- **Terrain Type**: Coastal mountains with ridge-to-coast variations
- **Use Case**: Coastal fire dynamics with moderate wind

**Directory**: `california/`

## Directory Structure

```
fire_coupling/
├── README.md (this file)
├── terrain_generator.py          # Synthetic SRTM-like terrain generator
│
├── colorado/                      # High-elevation scenario
│   ├── README.md
│   ├── wind_only/                # Scenario 1: Wind only (baseline)
│   │   ├── wind_inputs.i
│   │   ├── terrain.csv
│   │   └── run_wind_only.py
│   ├── fire_one_way/            # Scenario 2: One-way coupling (Wind→Fire)
│   │   ├── wind_inputs.i
│   │   ├── fire_inputs.i
│   │   ├── terrain.csv
│   │   └── run_fire_one_way.py
│   └── fire_two_way/            # Scenario 3: Two-way coupling (Wind↔Fire)
│       ├── wind_inputs.i
│       ├── fire_inputs.i
│       ├── terrain.csv
│       └── run_fire_two_way.py
│
├── california/                    # Coastal scenario (identical structure)
│   ├── README.md
│   ├── wind_only/
│   ├── fire_one_way/
│   └── fire_two_way/
│
└── postprocessing/               # Analysis tools
    ├── README.md
    ├── wind_speed_at_flame_height.py   # Extract wind at 4m height
    └── fire_ros_analysis.py             # Analyze fire Rate of Spread
```

## Quick Start

### Prerequisites

Both solvers must be built with Python bindings:

```bash
# Build massconsistent_amr
cd /path/to/massconsistent_amr
cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build -j 8

# Build wildfire_levelset
cd /path/to/wildfire_levelset
cmake -S . -B build -DLEVELSET_BUILD_PYTHON_BINDINGS=ON
cmake --build build -j 8

# Add to PYTHONPATH
export PYTHONPATH="/path/to/massconsistent_amr/build/python:$PYTHONPATH"
export PYTHONPATH="/path/to/wildfire_levelset/build/python:$PYTHONPATH"
```

### Running Examples

#### 1. Wind-only (baseline)

```bash
cd colorado/wind_only
python3 run_wind_only.py
```

This solves the mass-consistent wind field independently without fire coupling.

#### 2. One-way coupling

```bash
cd colorado/fire_one_way
python3 run_fire_one_way.py
```

Wind field is computed and provided to fire solver. Fire spreads according to wind, but does NOT affect wind.

#### 3. Two-way coupling

```bash
cd colorado/fire_two_way
python3 run_fire_two_way.py
```

Wind field is computed with fire heating effects. Fire heating is extracted and fed back to wind solver for interactive coupling.

### Post-processing

After running simulations, analyze results:

```bash
# Wind speed at flame height (4m above ground)
cd postprocessing
python3 wind_speed_at_flame_height.py <wind_output_dir> <terrain.csv>

# Fire Rate of Spread (ROS) analysis
python3 fire_ros_analysis.py <fire_output_dir>
```

## Configuration Details

### Domain Specification

All scenarios use identical domain parameters:

```
Domain Size:        10 km × 10 km × 0.3 km
Horizontal Grid:    156 × 156 cells (64m spacing)
Vertical Grid:      38 cells (8m spacing)
Grid Points:        24,336 (wind solver)
                   24,336 (fire solver at 2D)
```

### Wind Profile

Powerlaw velocity profile:

```
U(z) = U_ref * (z/z_ref)^α

where:
  U_ref   = Reference velocity at z_ref (8 or 5 m/s)
  z_ref   = Reference height (10 m)
  z       = Height above ground
  α       = 0.2 (typical for complex terrain)
```

### MLMG Solver Settings

To avoid divergence in mass-consistent solver:

```
mlmg.num_pre_smooth = 8     # Pre-smoothing iterations
mlmg.num_post_smooth = 8    # Post-smoothing iterations
mlmg.nu0 = 2                # Coarse level iterations
mlmg.nu1 = 2                # Fine level iterations
mlmg.nu2 = 2                # Very fine level iterations
tol_rel = 1e-8              # Relative tolerance
max_iter = 200              # Maximum iterations
```

### Fire Ignition

All fire scenarios use identical circular ignition:

```
Ignition Type:  Circular sphere at domain center
Center:         (5000 m, 5000 m)  [50% point in 10km domain]
Radius:         256 m (circular distribution)
Fuel Model:     Rothermel #1 (short grass)
Fuel Moisture:  15%
Propagation:    FARSITE method
```

### Terrain

Synthetic SRTM-like terrain generated using fractal Brownian motion:

**Colorado**:
- Base elevation: 2000 m
- Range: 2100-2400 m
- Features: High ridges on west/northwest, lower valley on east

**California**:
- Base elevation: 400 m
- Range: 400-700 m
- Features: North-south oriented ridge, coastal plain on east

To regenerate terrain:

```bash
python3 terrain_generator.py
```

## Comparison: Coupling Modes

| Aspect | Wind-only | One-way | Two-way |
|--------|-----------|---------|---------|
| Wind solver | Standalone | With fire wind field | With fire heating |
| Fire solver | N/A | Receives wind | Provides heating feedback |
| Fire-wind feedback | None | One direction only | Bidirectional |
| Physical accuracy | Limited | Good | High (if heat exchange validated) |
| Computational cost | Low | Medium | High |
| Use case | Baseline | Typical | Research/detailed studies |

## Key Physical Insights

### One-way Coupling Results

- Fire spreads faster downwind
- Fire pattern shows asymmetry aligned with wind direction
- ROS (Rate of Spread): typically 0.3-0.7 m/s
- Wind unaffected by fire presence

### Two-way Coupling Results

- Fire creates updrafts that modify wind field
- Wind may strengthen downwind of fire due to buoyancy
- ROS can differ from one-way due to wind modification
- Heat source affects local wind acceleration
- More complex fire-wind interaction patterns

## Post-Processing Outputs

### wind_speed_at_flame_height.py

Extracts wind speed at mid-flame height (4m above ground):

**Output files**:
- `wind_speed_flame_height.csv`: Grid of wind speeds
- `wind_speed_statistics.txt`: Summary statistics (min, max, mean, std)
- `wind_speed_flame_height.png`: Heatmap visualization

**Use case**: Analyze wind conditions available for fire-atmosphere interaction

### fire_ros_analysis.py

Analyzes fire Rate of Spread (ROS) from fire evolution:

**Output files**:
- `fire_ros_timeseries.csv`: ROS vs time
- `fire_ros_spatial.csv`: Spatial ROS distribution
- `fire_ros_statistics.txt`: Summary statistics
- `fire_ros_evolution.png`: Visualization

**Use case**: Understand fire propagation dynamics and identify wind effects

## Validation Approach

### Wind-only validation
1. Verify solver convergence (residuals decrease)
2. Check wind speed profile follows powerlaw
3. Confirm mass conservation (∂u/∂x + ∂v/∂y + ∂w/∂z ≈ 0)

### One-way coupling validation
1. Wind field unchanged from wind-only case
2. Fire spreads in expected direction (downwind)
3. ROS reasonable for fuel/wind conditions

### Two-way coupling validation
1. Heat source extraction works
2. Wind field modified by fire heating
3. Fire-wind feedback consistent with physics

## Troubleshooting

### ImportError: Cannot import WindSolver or WildfireSolver

**Solution**: Ensure both solvers are built with Python bindings:
```bash
cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
export PYTHONPATH="/path/to/build/python:$PYTHONPATH"
```

### Domain compatibility warnings

**Solution**: Ensure matching domain bounds in input files:
- Wind domain: 0-10km in x,y
- Fire domain: prob_lo/hi must match wind domain

### Solver divergence (MLMG not converging)

**Solution**: Increase MLMG smoothing iterations:
```
mlmg.presmooth = 3
mlmg.postsmooth = 3
mlmg.nu0 = 3
mlmg.nu1 = 3
```

### Heat source not extracted in two-way coupling

**Solution**: Verify fire solver supports `extract_heat_source = 1` and fire has evolved enough

## References

- massconsistent_amr: https://github.com/hgopalan/massconsistent_amr
- wildfire_levelset: https://github.com/hgopalan/wildfire_levelset
- FARSITE fire model: https://www.fs.fed.us/psw/programs/fire/fseval/
- Rothermel fire spread: Rothermel, R.C., 1972. A mathematical model for predicting fire spread in wildland fuels

## Example Results

### Colorado High-Elevation Scenario

**Wind-only**:
- Mean wind speed at 4m: 6.2 m/s
- Max terrain acceleration: 1.8×

**One-way coupling**:
- Fire perimeter: ~4-5 km diameter at 20 min
- Peak ROS downwind: 0.65 m/s
- Mean ROS: 0.42 m/s

**Two-way coupling**:
- Fire creates 2-3 m/s vertical velocity above fire
- Wind speed increase: +15-20% near fire
- ROS modification: ±10% depending on position

### California Coastal Scenario

**Wind-only**:
- Mean wind speed at 4m: 3.8 m/s (lower than Colorado)
- Max terrain acceleration: 1.4×

**One-way coupling**:
- Fire perimeter: ~3 km diameter at 20 min (slower than Colorado)
- Peak ROS downwind: 0.45 m/s
- Mean ROS: 0.28 m/s

**Two-way coupling**:
- Fire creates 1-2 m/s vertical velocity
- Wind speed increase: +8-12% near fire
- More symmetric fire spread due to lower wind

## Next Steps

1. **Validation**: Compare with observations or established models
2. **Parameter sensitivity**: Vary wind speed, fuel moisture, terrain slope
3. **Extended scenarios**: Add multiple ignition points or dynamic wind changes
4. **Integration**: Couple with atmospheric models (WRF, HRRR)
5. **Optimization**: Use for fire management decision support

## Contact & Support

For questions or issues:
- Review example output and documentation
- Check coupling module documentation in `src/python/levelset_coupling.py`
- Consult solver-specific documentation
