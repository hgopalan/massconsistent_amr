# Colorado Fire Coupling Scenario

High-elevation mountain terrain in southern Colorado with energetic westerly wind.

## Scenario Overview

**Location**: Southern Colorado (similar to Front Range near Boulder)

**Terrain Characteristics**:
- Elevation range: 2100-2400 m (high altitude)
- Ridge features: Strong ridge on western side, valley on eastern side
- Roughness: z0 = 0.1 m (typical for forested/complex terrain)

**Wind Conditions**:
- Reference wind: 8 m/s westerly at 10m height
- Wind profile: Powerlaw with α = 0.2
- Expected surface layer strength: Strong (energetic conditions)

**Fire Setup**:
- Fuel model: Short grass (Rothermel #1)
- Fuel moisture: 15%
- Ignition: Circular fire at domain center, 256m radius
- Expected burn behavior: Rapidly spreading downwind (eastward)

## Subdirectories

### wind_only/

Wind field solution without fire coupling. Serves as baseline for comparison.

**Run**:
```bash
cd wind_only
python3 run_wind_only.py
```

**Expected output**:
- Wind accelerates over ridges
- Flow acceleration factor: ~1.8x
- Mean surface wind: ~6-7 m/s at 4m height

### fire_one_way/

One-way coupling: wind → fire (fire does not affect wind)

**Configuration**:
- Wind field solves independently
- Fire solver receives 3D wind field
- Fire advances based on wind conditions
- Wind remains unchanged

**Run**:
```bash
cd fire_one_way
python3 run_fire_one_way.py
```

**Expected output**:
- Fire spreads rapidly downwind (eastward)
- Perimeter: ~4-5 km at 20 min
- ROS (Rate of Spread): 0.5-0.7 m/s downwind, 0.2-0.3 m/s upwind
- Mean ROS: ~0.4 m/s

### fire_two_way/

Two-way coupling: wind ↔ fire (fire heating affects wind)

**Configuration**:
- Wind field solves with fire heating source
- Fire solver receives modified wind field
- Fire heating creates updrafts
- Wind field modified by buoyancy effects

**Run**:
```bash
cd fire_two_way
python3 run_fire_two_way.py
```

**Expected output**:
- Fire creates strong vertical motion (2-3 m/s updrafts)
- Local wind speed increase: +15-20% near fire
- ROS modifications: ±10% from one-way case
- Heat-driven circulation patterns

## Configuration Parameters

### Wind Solver

```ini
# Domain
dx = 64.0           # 64 m horizontal resolution
dy = 64.0
dz = 8.0            # 8 m vertical resolution
domain_height = 300.0

# Wind profile
U_ref = 8.0         # 8 m/s reference wind
z_ref = 10.0        # 10 m reference height
powerlaw_exponent = 0.2

# Surface
z0 = 0.1            # Surface roughness 0.1 m

# MLMG Solver
mlmg.num_pre_smooth = 8
mlmg.num_post_smooth = 8
tol_rel = 1e-8
max_iter = 200
```

### Fire Solver

```ini
# Grid
n_cell_x = 156      # ~156 cells in 10km domain
n_cell_y = 156
prob_lo_x = 0.0
prob_lo_y = 0.0
prob_hi_x = 10000.0
prob_hi_y = 10000.0

# Ignition
source_type = sphere
center_x = 5000.0
center_y = 5000.0
sphere_radius = 256.0

# Fuel & Propagation
rothermel.model_number = 1
rothermel.fuel_moisture = 0.15
propagation_method = farsite
```

## Terrain

Synthetic SRTM-like terrain with:
- Fractal Brownian motion base
- Ridge feature on western side
- Valley on eastern side
- Realistic elevation variations

**Statistics**:
- Minimum elevation: 2098 m
- Maximum elevation: 2380 m
- Mean elevation: ~2200 m
- Relief: 282 m

To regenerate:
```bash
cd ..
python3 terrain_generator.py
```

## Analysis

### Wind-only Analysis

Examine convergence of wind solver:

```bash
cd wind_only
# Check convergence criteria
# Verify wind speed profile follows U(z) = U_ref * (z/z_ref)^0.2
# Confirm mass conservation
```

### One-way Coupling Analysis

Extract wind speed at flame height:

```bash
cd ../postprocessing
python3 wind_speed_at_flame_height.py \
    ../fire_one_way/wind_output \
    ../fire_one_way/terrain.csv \
    --height 4.0
```

Analyze fire Rate of Spread:

```bash
python3 fire_ros_analysis.py \
    ../fire_one_way/fire_output
```

Expected ROS statistics:
- Mean: 0.35-0.45 m/s
- Max: 0.60-0.75 m/s (downwind)
- Min: 0.15-0.25 m/s (upwind/flanks)

### Two-way Coupling Analysis

Compare outputs between one-way and two-way:

```bash
cd ../postprocessing

# Wind speed comparison
python3 wind_speed_at_flame_height.py \
    ../fire_one_way/wind_output \
    ../fire_one_way/terrain.csv

python3 wind_speed_at_flame_height.py \
    ../fire_two_way/wind_output \
    ../fire_two_way/terrain.csv

# Fire ROS comparison
python3 fire_ros_analysis.py \
    ../fire_one_way/fire_output

python3 fire_ros_analysis.py \
    ../fire_two_way/fire_output
```

**Expected differences**:
- Wind speed increase: 15-20% in fire zone
- ROS increase: 5-15% due to wind modification
- Vertical motion: 2-3 m/s updrafts
- Heat plume: Extends 500-1000 m above fire

## Physical Interpretation

### Colorado Conditions

**High-elevation physics**:
1. Thinner atmosphere (lower air density)
2. Stronger wind acceleration over complex terrain
3. More vigorous fire-atmosphere interaction
4. Potential for pyrocumulus cloud development

**Expected fire behavior**:
1. Rapid spread downwind (east) at 0.6-0.7 m/s
2. Lateral spread slower, ~0.3 m/s
3. Very limited upwind spread, <0.1 m/s
4. Strong fire-wind alignment

### Wind-Fire Coupling Effects

**One-way coupling**:
- Wind field unchanged (baseline reference)
- Fire follows pure wind-driven spread

**Two-way coupling**:
- Fire heating creates local pressure perturbation
- Buoyancy-driven updrafts modify wind
- Could trigger vortex formation
- Heat feedback increases ROS by 10-15%

## Troubleshooting

### Wind solver divergence

If MLMG solver fails to converge:
- Increase smoothing iterations (presmooth, postsmooth)
- Reduce tolerance (tol_rel) if needed
- Check terrain smoothness

### Fire spread too fast/slow

Adjust fuel moisture:
```ini
rothermel.fuel_moisture = 0.15  # 15% (default)
rothermel.fuel_moisture = 0.10  # 10% (drier, faster spread)
rothermel.fuel_moisture = 0.20  # 20% (wetter, slower spread)
```

### Two-way coupling heat source issues

Verify fire solver extracts heat correctly:
```ini
extract_heat_source = 1
heat_source_height = 5.0  # Extract at 5m height
```

## Comparison with Other Scenarios

| Parameter | Colorado | California |
|-----------|----------|-----------|
| Elevation | 2100-2400 m | 400-700 m |
| Wind speed | 8 m/s | 5 m/s |
| Terrain relief | 282 m | 286 m |
| Expected ROS | 0.40-0.45 m/s | 0.25-0.35 m/s |
| Coupling strength | Strong | Moderate |
| Fire-wind feedback | Significant | Moderate |

## References

- Terrain data: Synthetic SRTM-like elevation model
- Fuel model: Rothermel, R.C., 1972
- Wind profile: Monin-Obukhov similarity theory
- Fire-wind coupling: Based on wildfire_levelset framework

## Next Steps

1. **Sensitivity analysis**: Vary wind speed, fuel moisture
2. **Parameter studies**: Investigate MLMG solver settings
3. **Validation**: Compare with idealized benchmark cases
4. **Extension**: Add buildings/structures for urban fire scenarios
5. **Integration**: Couple with mesoscale atmospheric model
