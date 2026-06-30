# California Fire Coupling Scenario

Moderate-elevation coastal terrain in northern California with moderate northwesterly wind.

## Scenario Overview

**Location**: Northern California coast (similar to Marin County, Bay Area region)

**Terrain Characteristics**:
- Elevation range: 400-700 m (moderate elevation)
- Ridge features: North-south oriented coastal ridge, transitioning to coastal plain
- Roughness: z0 = 0.08 m (coastal vegetation, less dense than Colorado)

**Wind Conditions**:
- Reference wind: 5 m/s northwesterly at 10m height (typical sea breeze/coastal flow)
- Wind profile: Powerlaw with α = 0.2
- Expected surface layer strength: Moderate (typical coastal conditions)

**Fire Setup**:
- Fuel model: Short grass (Rothermel #1)
- Fuel moisture: 15%
- Ignition: Circular fire at domain center, 256m radius
- Expected burn behavior: Steady spread downwind with moderate ROS

## Subdirectories

### wind_only/

Wind field solution without fire coupling. Serves as baseline for comparison.

**Run**:
```bash
cd wind_only
python3 run_wind_only.py
```

**Expected output**:
- Wind accelerates over ridge but less than Colorado
- Flow acceleration factor: ~1.4x
- Mean surface wind: ~3.5-4 m/s at 4m height (lower wind speed)

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
- Fire spreads moderately downwind (northwesterly direction)
- Perimeter: ~3 km at 20 min (slower than Colorado due to lower wind)
- ROS (Rate of Spread): 0.3-0.45 m/s downwind, 0.15-0.25 m/s upwind
- Mean ROS: ~0.25-0.30 m/s (significantly slower than Colorado)

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
- Fire creates moderate vertical motion (1-2 m/s updrafts, less than Colorado)
- Local wind speed increase: +8-12% near fire
- ROS modifications: ±5-8% from one-way case (smaller feedback than Colorado)
- More symmetric fire spread due to lower ambient wind

## Configuration Parameters

### Wind Solver

```ini
# Domain
dx = 64.0           # 64 m horizontal resolution
dy = 64.0
dz = 8.0            # 8 m vertical resolution
domain_height = 300.0

# Wind profile
U_ref = 5.0         # 5 m/s reference wind (lower than Colorado)
z_ref = 10.0        # 10 m reference height
powerlaw_exponent = 0.2

# Surface
z0 = 0.08           # Surface roughness 0.08 m (coastal)

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
- North-south oriented coastal ridge
- Coastal plain features on eastern side
- Realistic elevation variations

**Statistics**:
- Minimum elevation: 418 m
- Maximum elevation: 703 m
- Mean elevation: ~550 m
- Relief: 285 m

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
# Confirm mass conservation with lower reference wind
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
- Mean: 0.25-0.35 m/s (significantly slower than Colorado)
- Max: 0.40-0.50 m/s (downwind)
- Min: 0.10-0.20 m/s (upwind/flanks)

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
- Wind speed increase: 8-12% in fire zone (smaller than Colorado)
- ROS increase: 3-8% due to wind modification
- Vertical motion: 1-2 m/s updrafts (less vigorous)
- Heat plume: Extends 300-700 m above fire (less extensive)

## Physical Interpretation

### California Coastal Physics

**Lower-elevation conditions**:
1. Denser atmosphere (higher air density)
2. Moderate wind acceleration over gentle terrain
3. Less vigorous fire-atmosphere interaction
4. Lower potential for strong convection

**Typical coastal fire behavior**:
1. Steady spread at moderate pace, 0.3-0.4 m/s downwind
2. More symmetric spread pattern (lower ambient wind directionality)
3. Lateral spread more significant, ~0.2-0.3 m/s
4. Upwind propagation minimal but non-negligible

### Wind-Fire Coupling Effects

**One-way coupling**:
- Wind field unchanged (baseline reference)
- Fire spread driven purely by wind direction/speed
- Fire pattern shows some asymmetry aligned with wind

**Two-way coupling**:
- Fire heating creates modest pressure perturbation
- Buoyancy-driven updrafts gently modify wind
- Heat feedback increases ROS by 5-8% (less than Colorado)
- More subtle fire-wind interaction patterns

## Comparison: Colorado vs California

| Aspect | Colorado | California |
|--------|----------|-----------|
| Elevation | High (2100-2400m) | Moderate (400-700m) |
| Wind speed | Strong (8 m/s) | Moderate (5 m/s) |
| Wind acceleration | 1.8× | 1.4× |
| Mean ROS (one-way) | 0.40-0.45 m/s | 0.25-0.35 m/s |
| Wind effect on ROS | Strong | Moderate |
| Two-way feedback | 15-20% | 8-12% |
| Updraft strength | 2-3 m/s | 1-2 m/s |
| Fire spreading time | Fast | Moderate |
| Physical regime | Energetic | Coastal/moderate |

## Troubleshooting

### Wind solver divergence

If MLMG solver fails to converge:
- Increase smoothing iterations (presmooth, postsmooth)
- Check that U_ref = 5.0 is used (not 8.0)
- Verify terrain smoothness

### Fire spread too slow

With lower wind speed (5 m/s), slower spread is expected. To increase:
- Reduce fuel moisture:
  ```ini
  rothermel.fuel_moisture = 0.10  # Drier conditions
  ```
- Increase wind speed (if modeling different conditions):
  ```ini
  U_ref = 7.0  # Higher wind
  ```

### Two-way coupling weak effects

Lower wind speed means weaker coupling. To enhance:
- Increase wind speed
- Decrease fuel moisture (more intense fire)
- Verify heat source extraction is working

## Calibration Notes

### For Real Fire Events

To apply this template to actual California fires:

1. **Obtain real SRTM terrain**:
   ```bash
   # Download SRTM tiles for fire location
   # Use geographic_data_fetcher.py for automated download
   ```

2. **Get wind observations**:
   ```bash
   # Use HRRR, NAM, or local weather stations
   # Extract U_ref at 10m height for your fire
   ```

3. **Determine fuel characteristics**:
   ```bash
   # Use LANDFIRE fuel models
   # Update rothermel.model_number for actual fuel type
   ```

4. **Validate against observations**:
   - Compare predicted ROS with fire progression
   - Adjust fuel moisture to match observed spread

## References

- Terrain data: Synthetic SRTM-like elevation model
- Fuel model: Rothermel, R.C., 1972
- Coastal wind: Typical California sea breeze/coastal winds
- Fire-wind coupling: Based on wildfire_levelset framework

## Next Steps

1. **Coastal-specific physics**: Add sea breeze modeling
2. **Multi-scale coupling**: Integrate with mesoscale models (WRF)
3. **Real event simulation**: Apply to documented California fires
4. **Sensitivity studies**: Vary initial moisture, wind speed
5. **Comparison**: Validate against other fire models (FARSITE, QUIC-Fire)
