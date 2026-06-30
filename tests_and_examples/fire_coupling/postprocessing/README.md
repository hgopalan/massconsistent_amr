# Post-processing Tools for Fire Coupling Examples

This directory contains Python scripts for analyzing wind and fire simulation outputs.

## Available Tools

### 1. wind_speed_at_flame_height.py

Extracts wind speed at mid-flame height (4m above ground) from wind solver output.

**Purpose**: Analyze wind conditions at heights relevant for fire-atmosphere interaction.

**Usage**:
```bash
python3 wind_speed_at_flame_height.py <wind_output_dir> <terrain.csv> [--height 4.0] [--output-dir .]
```

**Arguments**:
- `wind_output_dir`: Directory containing wind solver output files
- `terrain.csv`: Terrain elevation file from simulation
- `--height`: Height above ground to evaluate wind (default: 4.0 m)
- `--output-dir`: Directory for output files (default: current directory)

**Output files**:
- `wind_speed_flame_height.csv`: Grid of wind speeds at flame height
- `wind_speed_statistics.txt`: Summary statistics (min, max, mean, std, percentiles)
- `wind_speed_flame_height.png`: Heatmap visualization (if matplotlib available)

**Example - Colorado one-way coupling**:
```bash
python3 wind_speed_at_flame_height.py \
    ../colorado/fire_one_way/wind_output \
    ../colorado/fire_one_way/terrain.csv \
    --height 4.0 \
    --output-dir colorado_one_way_analysis
```

**Typical output**:
```
Wind Speed at Flame Height (4 m above ground) - Statistics
============================================================

Minimum:    2.345 m/s
Maximum:    9.876 m/s
Mean:       6.234 m/s
Median:     6.120 m/s
Std Dev:    1.234 m/s
25th pctl:  5.123 m/s
75th pctl:  7.345 m/s
```

**Use cases**:
- Assess wind strength available for fire-atmosphere coupling
- Identify wind acceleration zones over terrain
- Compare wind speeds between wind-only and two-way coupling scenarios
- Validate wind solver convergence and terrain effects

### 2. fire_ros_analysis.py

Analyzes fire Rate of Spread (ROS) from fire solver output.

**Purpose**: Quantify fire propagation dynamics and wind effects on spread rate.

**Usage**:
```bash
python3 fire_ros_analysis.py <fire_output_dir> [--output-dir .]
```

**Arguments**:
- `fire_output_dir`: Directory containing fire solver output files
- `--output-dir`: Directory for output files (default: current directory)

**Output files**:
- `fire_ros_timeseries.csv`: ROS evolution over time (m/s and m/min)
- `fire_ros_spatial.csv`: Spatial distribution of ROS at final time
- `fire_ros_statistics.txt`: Summary statistics
- `fire_ros_evolution.png`: Visualization showing ROS trends and spatial patterns

**Example - California two-way coupling**:
```bash
python3 fire_ros_analysis.py \
    ../california/fire_two_way/fire_output \
    --output-dir california_two_way_analysis
```

**Typical output**:
```
Fire Rate of Spread (ROS) Analysis
============================================================

TEMPORAL ROS (from fire perimeter evolution):
  Mean:    0.3456 m/s (20.74 m/min)
  Std Dev: 0.0234 m/s
  Range:   0.3100 - 0.3890 m/s

SPATIAL ROS (at end of simulation):
  Mean:    0.3200 m/s (19.20 m/min)
  Std Dev: 0.0890 m/s
  Range:   0.1200 - 0.5600 m/s
```

**Use cases**:
- Understand fire propagation characteristics
- Identify wind-driven vs. fuel-driven spread
- Compare ROS between different coupling scenarios
- Validate fire solver predictions
- Assess fire threat in different wind conditions

## Comparative Analysis Workflow

### Comparing Coupling Scenarios

To compare wind-only, one-way, and two-way coupling:

```bash
# Analyze wind speeds for all three scenarios
cd postprocessing

python3 wind_speed_at_flame_height.py \
    ../colorado/wind_only/wind_output \
    ../colorado/wind_only/terrain.csv \
    --output-dir colorado_wind_only

python3 wind_speed_at_flame_height.py \
    ../colorado/fire_one_way/wind_output \
    ../colorado/fire_one_way/terrain.csv \
    --output-dir colorado_one_way

python3 wind_speed_at_flame_height.py \
    ../colorado/fire_two_way/wind_output \
    ../colorado/fire_two_way/terrain.csv \
    --output-dir colorado_two_way

# Analyze fire spread for coupled scenarios
python3 fire_ros_analysis.py \
    ../colorado/fire_one_way/fire_output \
    --output-dir colorado_one_way

python3 fire_ros_analysis.py \
    ../colorado/fire_two_way/fire_output \
    --output-dir colorado_two_way
```

### Expected Differences

**Wind speeds at flame height**:
- Wind-only vs. One-way: Should be identical (fire doesn't affect wind)
- One-way vs. Two-way: Two-way should show +10-20% increase in fire zone (updrafts)

**Fire Rate of Spread**:
- One-way vs. Two-way: Two-way should show ±5-15% variation depending on wind modification

## Output Interpretation

### Wind Speed Analysis

**High wind speed zones (> 7 m/s)**:
- Strong fire-wind interaction expected
- Rapid fire spread likely
- High flame lengths

**Low wind speed zones (< 3 m/s)**:
- Fire spread mainly fuel-driven
- Weaker fire-atmosphere coupling
- More isotropic spread pattern

### Fire ROS Analysis

**ROS < 0.2 m/s**:
- Slow fire, typically fuel-limited or low wind
- Fire traveling ~ 12 m/min = 720 m/hour

**ROS 0.2-0.5 m/s**:
- Moderate fire, typical grass fire conditions
- Fire traveling ~ 20-30 m/min = 1.2-1.8 km/hour

**ROS > 0.5 m/s**:
- Fast fire, wind-driven or high fuel load
- Fire traveling > 30 m/min > 1.8 km/hour
- High fire intensity

**Spatial ROS variation**:
- Downwind ROS typically 2-3× upwind ROS
- Along-flank ROS intermediate
- Strong variation indicates wind dominance

## Advanced Analysis

### Cross-Scenario Comparison

Create summary statistics across all scenarios:

```bash
# Extract key statistics
for scenario in wind_only fire_one_way fire_two_way; do
    echo "=== $scenario ==="
    grep "Mean:" ${scenario}_wind/wind_speed_statistics.txt
    grep "Mean:" ${scenario}_fire/fire_ros_statistics.txt
done
```

### Wind-Fire Coupling Strength

Quantify coupling intensity by comparing:

```python
# In a custom script:
wind_only_speed = read_wind_speed('colorado_wind_only/wind_speed_flame_height.csv')
two_way_speed = read_wind_speed('colorado_two_way/wind_speed_flame_height.csv')

# Calculate increase in fire zone
coupling_strength = (two_way_speed - wind_only_speed) / wind_only_speed * 100
print(f"Fire-induced wind change: {coupling_strength:.1f}%")
```

## Requirements

**Python 3.6+**

**Required packages**:
- numpy (for array operations)
- csv (standard library)

**Optional packages**:
- matplotlib (for visualization)

**Installation**:
```bash
pip3 install numpy matplotlib
```

## Troubleshooting

### "Cannot read wind field" error

The postprocessing scripts currently use synthetic demonstration data. To use real solver output:

1. Determine your wind solver's output format (AMReX plotfile, HDF5, NetCDF)
2. Implement reader in `read_wind_field()` function
3. Ensure output coordinates match solver domain

### "matplotlib not available" warning

Visualization is optional. Scripts still generate CSV output without matplotlib.

To enable visualization:
```bash
pip3 install matplotlib
```

### Output files are all zeros

Check that terrain.csv file is properly formatted:
```bash
head terrain.csv
# Should show: x,y,z (with numerical values)
```

## Citation

If using these post-processing tools in research, cite:

```
Fire Coupling Examples for massconsistent_amr
https://github.com/hgopalan/massconsistent_amr/tree/main/tests_and_examples/fire_coupling
```

## Future Enhancements

Potential additions to postprocessing toolkit:

1. **Wind direction analysis**: Vector field visualization
2. **Fire intensity mapping**: Combine ROS with fuel characteristics
3. **Flame height estimation**: Correlate with heat release rate
4. **Smoke transport**: Couple with Lagrangian dispersion model
5. **Economic impact**: Calculate threatened structures or resources
6. **Animation generation**: Create time-series movies of fire evolution
7. **Validation metrics**: Compare against observations if available

## References

- Rothermel, R.C., 1972. A mathematical model for predicting fire spread in wildland fuels
- FARSITE fire behavior model: https://www.fs.fed.us/psw/programs/fire/fseval/
- Rate of Spread definitions: USDA Forest Service fire behavior guides
