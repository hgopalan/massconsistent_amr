# Feature Migration Guide: From Simple Puff to CALPUFF-like Complexity

## Overview

This guide helps users progressively enhance their dispersion models from basic single-source Gaussian puff to full multi-source, chemistry-enabled, CALPUFF-like complexity. Each step is backward compatible and can be adopted incrementally.

## Step 1: Basic Single-Source Dispersion (Default Behavior)

**Complexity:** ⭐ (Minimal)

**Features:**
- Single point source
- Constant emission rate
- Log-law wind profile
- Basic Gaussian diffusion
- Pasquill-Gifford stability

**Minimal inputs.i:**
```ini
# Basic domain
xmin = -500.0
xmax = 5000.0
ymin = -500.0
ymax = 5000.0
zmin = 0.0
zmax = 500.0

# Wind field (constant)
init_mode = loglaw
U_ref = 10.0
z_ref = 10.0
z0 = 0.1

# Single puff source
puff_model.enabled = true
puff_model.source_x = 100.0
puff_model.source_y = 100.0
puff_model.source_z = 50.0
puff_model.emission_rate = 1.0
puff_model.emission_duration = 3600.0
puff_model.K_h = 1.0
puff_model.K_v = 0.5
```

**Output:**
```
receptor_concentration.csv
```

**Validation:** Against simple Gaussian plume solutions

---

## Step 2: Add Plume Rise Physics

**Complexity:** ⭐⭐ (Low)

**New Features:**
- Briggs plume rise for buoyant sources
- Stack temperature and exit velocity
- Effective stack height calculation

**Changes to inputs.i:**
```ini
# Add to puff_model section:
puff_model.enable_plume_rise = true
puff_model.heat_flux = 100.0  # [m⁴/s³]
# OR for stack parameters:
# stack_diameter = 1.5  [m]
# stack_exit_velocity = 12.0  [m/s]
# stack_exit_temperature = 350.0  [K]
```

**Expected Impact:**
- Plume rises above source height
- Greater initial mixing
- Higher ground-level concentration offset downwind

**Testing:**
```bash
# Compare with/without plume rise
./wind_solver inputs_norise.i
./wind_solver inputs_withrise.i
# Ground concentrations should differ by factor ~2-3 near source
```

---

## Step 3: Add Time-Varying Emissions

**Complexity:** ⭐⭐ (Low)

**New Features:**
- Daily traffic cycle
- Episodic events
- Time-dependent source strength

**Setup:**
```bash
# Generate time-varying profile
python src/python/emission_profile_generator.py --profile traffic \
  --base-rate 1.0 --peak-factor 2.5 --duration 86400 \
  --output emissions_daily.csv
```

**Changes to inputs.i:**
```ini
puff_model.emissions_timeseries_file = emissions_daily.csv
```

**Expected Impact:**
- Concentrations vary with emission rate
- Morning and evening peaks in traffic scenario
- Time-averaged impact reduced vs. constant emissions

**Testing:**
```bash
# Check emission rates are being read
grep -A 5 "Emission timeseries" console_output.txt
```

---

## Step 4: Add Terrain Reflection

**Complexity:** ⭐⭐⭐ (Medium)

**New Features:**
- Image source method for ground reflection
- Terrain-dependent wind field
- Complex terrain effects

**Setup:**
```bash
# Create terrain file
python wind_field_converter.py --uniform 10.0 0.0 0.0 --output wind_field.csv
```

**Changes to inputs.i:**
```ini
terrain_file = terrain.csv
puff_model.enable_terrain_reflection = true
puff_model.use_image_source = true
```

**Expected Impact:**
- Higher ground-level concentrations (image source interference)
- Wind field modified by topography
- Concentration field more realistic near ground

**Testing:**
```bash
# Verify image source is active
grep "Terrain reflection" console_output.txt
# Compare z-profiles with/without terrain
```

---

## Step 5: Add Multi-Source Support

**Complexity:** ⭐⭐⭐ (Medium)

**New Features:**
- Multiple emission sources
- Different source types (point, line, area)
- Source superposition

**Setup:**
```bash
# Create multi-source file
cat > sources.csv << 'EOF'
source_id,x,y,z,type,emission_rate,emission_duration,stack_diameter,stack_exit_velocity,stack_exit_temperature
source_1,100.0,100.0,50.0,point,1.0,86400,1.5,12.0,350.0
source_2,500.0,100.0,25.0,point,0.5,86400,0.8,8.0,310.0
source_3,100.0,500.0,10.0,point,0.3,86400,0.3,2.0,298.0
EOF
```

**Changes to inputs.i:**
```ini
puff_model.sources_file = sources.csv
```

**Expected Impact:**
- Contributions from multiple sources visible in receptor output
- Downwind plume shows multiple peaks
- Total concentration is superposition

**Testing:**
```bash
# Verify sources are loaded
grep "Loaded.*sources" console_output.txt

# Extract and plot individual source contributions
python -c "
import csv
with open('receptor_concentration.csv') as f:
    rows = list(csv.DictReader(f))
    for r in rows[:5]:
        print(f\"x={r['x']}, y={r['y']}, C={r['concentration']}\")
"
```

---

## Step 6: Add Chemical Reactions

**Complexity:** ⭐⭐⭐⭐ (High)

**New Features:**
- Multi-species reactive chemistry
- Temperature/humidity-dependent rates
- Gas-particle partitioning

**Setup:**
```bash
# Build chemistry matrix
python src/python/chemistry_builder.py --template soxnox \
  --output chemistry.csv
```

**Changes to inputs.i:**
```ini
puff_model.enable_reactive_chemistry = true
puff_model.chemistry_file = chemistry.csv
puff_model.chemistry_timestep = 1.0
puff_model.enable_temperature_dependent_rates = true
puff_model.enable_rh_dependent_rates = true
```

**Expected Impact:**
- SO₂ converts to SO₄ downwind
- NOₓ converts to HNO₃/NO₃⁻
- Concentration decreases slower (redistribution among species)
- Temperature/humidity affects reaction rates

**Testing:**
```bash
# Compare total vs. species concentrations
python visibility_postprocessor.py --input receptor_concentration.csv \
  --output visibility.csv --species SO4,NO3
```

---

## Step 7: Add Deposition Mechanisms

**Complexity:** ⭐⭐⭐⭐ (High)

**New Features:**
- Dry deposition (size/surface dependent)
- Wet deposition (precipitation scavenging)
- Particle settling

**Setup:**
```bash
# Use default or custom deposition parameters
# Default values already embedded in code
```

**Changes to inputs.i:**
```ini
puff_model.enable_settling = true
puff_model.enable_puff_deposition = true
puff_model.enable_wet_deposition = true

# Optional: custom parameters
puff_model.deposition_params_file = deposition_params.csv
```

**Expected Impact:**
- Concentrations decrease more rapidly downwind
- Larger particles deposit near source
- Precipitation events cause rapid removal
- Ground-level deposition patterns

**Testing:**
```bash
# Check deposition flux in output
grep "deposition" receptor_concentration.csv | head -5

# Compare mass balance with/without deposition
total_emitted = 1.0 * 86400
grep "Total deposited" console_output.txt
```

---

## Step 8: Add Visibility Metrics

**Complexity:** ⭐⭐⭐ (Medium-High)

**New Features:**
- IMPROVE extinction algorithm
- Visual range and deciview
- Fog/icing probability

**Setup:**
```bash
# No special setup needed - post-processing step
```

**Changes to inputs.i:**
```ini
puff_model.enable_optical_properties = true
puff_model.compute_visibility_at_receptors = true
```

**Post-processing:**
```bash
python src/python/visibility_postprocessor.py \
  --input receptor_concentration.csv \
  --output visibility_metrics.csv \
  --species SO4,NO3 \
  --baseline 200 \
  --report impact_summary.txt
```

**Expected Impact:**
- Visibility reduced downwind of source
- Visual range correlates with concentration
- Deciview scale more intuitive for impacts

**Testing:**
```bash
# Check visibility statistics
cat impact_summary.txt | grep -A 10 "Visual Range"
```

---

## Step 9: Complex Meteorology (Spatial Variations)

**Complexity:** ⭐⭐⭐⭐⭐ (Very High)

**New Features:**
- Spatially-varying wind profiles
- Height-dependent stability
- Complex boundary layer

**Setup:**
```bash
# Create spatial meteorology file
cat > met_profiles.csv << 'EOF'
profile_id,x_ref,y_ref,z_agl,u,v,w,K_h,K_v,stability_class
profile_1,100,100,10,10.0,2.0,0.1,1.0,0.5,D
profile_1,100,100,50,12.0,2.5,0.2,2.0,1.0,D
...
EOF
```

**Changes to inputs.i:**
```ini
puff_model.met_profiles_file = met_profiles.csv
puff_model.enable_height_dependent_K = true
```

**Expected Impact:**
- Wind direction/speed varies with height
- Diffusivity K varies vertically
- More realistic plume shape
- Better match to observations

**Testing:**
```bash
# Verify profiles are loaded
grep "Loaded.*meteorology profiles" console_output.txt

# Check wind field at different heights
```

---

## Full CALPUFF-like Configuration

**Complexity:** ⭐⭐⭐⭐⭐ (Maximum)

**All features combined:**
```ini
# Domain and basic setup
xmin = -2000.0
xmax = 20000.0
ymin = -2000.0
ymax = 20000.0
zmin = 0.0
zmax = 2000.0

terrain_file = terrain.csv
init_mode = windfield
windfield_file = wind_field.csv

# Enable all Phase 3 physics
puff_model.enabled = true
puff_model.sources_file = sources_multisource.csv
puff_model.emissions_timeseries_file = emissions_timeseries.csv
puff_model.met_profiles_file = met_profiles_spatial.csv
puff_model.receptors_file = receptors_grid.csv

# Plume rise
puff_model.enable_plume_rise = true

# Terrain effects
puff_model.enable_terrain_reflection = true
puff_model.use_image_source = true

# Wind shear
puff_model.enable_wind_shear = true
puff_model.wind_shear_coefficient = 0.05
puff_model.veer_angle = 15.0

# Chemistry
puff_model.enable_reactive_chemistry = true
puff_model.chemistry_file = chemistry.csv
puff_model.enable_temperature_dependent_rates = true
puff_model.enable_rh_dependent_rates = true

# Deposition
puff_model.enable_settling = true
puff_model.enable_puff_deposition = true
puff_model.enable_wet_deposition = true
puff_model.deposition_params_file = deposition_params.csv

# Visibility
puff_model.enable_optical_properties = true
puff_model.compute_visibility_at_receptors = true

# Output
puff_model.receptor_output_file = concentrations.csv
puff_model.output_frequency = 300.0
```

**Workflow:**
```bash
# 1. Prepare all CSV files
python receptor_grid_generator.py --grid 2d --nx 50 --ny 50 --output receptors_grid.csv
python emission_profile_generator.py --profile traffic --output emissions_timeseries.csv
python chemistry_builder.py --template full --output chemistry.csv

# 2. Run solver
./wind_solver inputs_full.i

# 3. Post-process results
python visibility_postprocessor.py --input concentrations.csv \
  --output visibility_metrics.csv --report impact_summary.txt

# 4. Generate plots/reports
# (user's own scripts)
```

---

## Tuning Guide

### When to Use Each Feature

| Feature | When to Use | When NOT to Use |
|---------|-----------|-----------------|
| Plume Rise | Elevated sources (T > 310 K or V > 10 m/s) | Ground-level sources |
| Time-varying | Episodic/changing conditions | Continuous steady-state |
| Terrain | Complex/mountainous terrain | Flat terrain |
| Multi-source | Multiple distinct sources | Single source only |
| Chemistry | Regional/long-range transport | Near-field only (<1 km) |
| Deposition | Long-range model (>10 km) | Very close to source (<1 km) |
| Spatial met. | Complex meteorology | Homogeneous conditions |

### Performance-Accuracy Tradeoffs

**Fast (simple):**
```
- Single source
- No chemistry
- No deposition
- Constant emissions
- ~10 ms per output step
```

**Medium (balanced):**
```
- Multi-source (3-5)
- Chemistry enabled
- Deposition
- ~50-100 ms per output step
```

**Slow (comprehensive):**
```
- Many sources (10+)
- Complex chemistry
- Detailed deposition
- Spatial meteorology
- ~500+ ms per output step
```

### Accuracy Improvements

| Feature | Accuracy Gain |
|---------|---|
| Plume rise | 2-5× at elevated receptors |
| Multi-source | ~50% for industrial complex |
| Chemistry | 10-50% for species concentrations |
| Deposition | 20-100% depending on distance |
| Terrain | 2-10× for complex terrain |

---

## Common Pitfalls

### Pitfall 1: Unrealistic Emission Rates

**Problem:** Concentrations are 100× higher than expected

**Solution:**
- Check units consistency (kg/s vs g/s vs moles/s)
- Verify emission_rate matches source strength
- Compare with regulatory standards (e.g., 1 ton/year = 0.032 g/s)

### Pitfall 2: Chemistry Too Slow

**Problem:** SO₂ not converting to SO₄ downwind

**Solution:**
- Check chemistry_file is being loaded
- Verify rate constants are reasonable (typically 1e-4 to 1e-3 s⁻¹)
- Increase K_h/K_v for faster mixing

### Pitfall 3: Deposition Removes Everything

**Problem:** Concentrations near zero after 5 km

**Solution:**
- Check deposition velocity parameters
- Use default values first, then tune
- Verify precipitation rate (if wet deposition)

### Pitfall 4: Performance Too Slow

**Problem:** Solver takes hours for single scenario

**Solution:**
- Reduce number of receptors
- Increase output frequency (fewer outputs)
- Disable features not needed (chemistry, deposition)
- Use coarser wind field grid

---

## References

- Turner et al. (1994): Workbook of Atmospheric Dispersion Estimates
- Seinfeld & Pandis (2016): Atmospheric Chemistry and Physics (3rd ed.)
- EPA CALPUFF Documentation
- Briggs (1973): Plume Rise Equations
- IMPROVE Algorithm (Pitchford et al., 2007)
