# Phase 4: CSV Input/Output Infrastructure Guide

## Overview

Phase 4 extends the massconsistent_amr dispersion model with a unified CSV input/output pipeline. This enables flexible configuration of complex multi-source, time-varying emission scenarios while maintaining backward compatibility with existing inputs.i workflows.

## Phase 4.1: Unified CSV Input Pipeline

### Supported CSV Input Files

All CSV input files are **optional**. If not specified, sensible defaults apply.

#### 1. **sources.csv** - Multi-Source Emission Definitions

Define multiple emission sources with different types and characteristics.

**Format:**
```csv
source_id,x [m],y [m],z [m],type,emission_rate [units/s],emission_duration [s],stack_diameter [m],stack_exit_velocity [m/s],stack_exit_temperature [K]
source_1,100.0,150.0,50.0,point,1.0,86400.0,1.5,12.0,350.0
source_2,200.0,200.0,25.0,point,0.5,86400.0,0.8,8.0,310.0
```

**Source types:**
- `point`: Single point source (industrial stack, vent)
- `line`: Linear source along a line (road, fence)
- `area`: 2D area source (parking lot, pond)
- `volume`: 3D volume source (large tank, excavation)

**Key parameters:**
- `source_id`: Unique identifier (string, no spaces)
- `x, y, z`: Source location [m] in domain coordinates
- `type`: One of: point, line, area, volume
- `emission_rate`: [units/s] - emission strength
- `emission_duration`: [s] - how long source is active
- `stack_diameter`: [m] - exit diameter (0 for fugitive sources)
- `stack_exit_velocity`: [m/s] - momentum exit velocity
- `stack_exit_temperature`: [K] - buoyancy heat flux

**Example:** See `docs/examples/sources_multisource_new.csv`

#### 2. **emissions_time_series.csv** - Time-Varying Emission Rates

Define how emission rates vary over time. Useful for:
- Daily traffic cycles (rush hour peaks)
- Industrial shift changes
- Episodic events (accidents, maintenance)
- Seasonal variations (heating/cooling)

**Format:**
```csv
time [s],emission_rate [units/s],description
0.0,1.0,Initial
3600.0,1.5,Morning rush
7200.0,1.2,Mid-morning
10800.0,0.5,Afternoon dip
```

**Behavior:**
- Times must be in ascending order
- Emission rates are interpolated linearly between specified times
- Before first time: uses first rate
- After last time: uses last rate
- Can span any duration (hours, days, years)

**Example:** See `docs/examples/emissions_time_series.csv`

#### 3. **deposition_params.csv** - Particle Dry/Wet Deposition

Specify size-dependent and species-specific deposition parameters.

**Format:**
```csv
# Dry Deposition
particle_id,diameter [um],density [kg/m3],vd_grass [m/s],vd_urban [m/s],vd_water [m/s],vd_forest [m/s]
SO4_fine,0.5,1800.0,0.001,0.002,0.0005,0.003

# Wet Scavenging
species_id,lambda0_base [1/s],precip_exponent
SO4_fine,1.0e-4,0.80
```

**Dry deposition parameters:**
- Deposition velocity varies by surface type
- Grass/urban/water/forest surfaces have different roughness
- Larger particles have higher deposition velocities

**Wet deposition (scavenging):**
- Scavenging follows: Λ(P) = Λ₀ × (P / P_ref)^a
- P = precipitation rate [mm/hr]
- P_ref = 0.1 mm/hr (reference)
- a = precipitation exponent (typically 0.6-0.9)

**Example:** See `docs/examples/deposition_params.csv`

#### 4. **met_profiles.csv** - Spatial Meteorology

Define vertically-varying wind and diffusivity profiles at different locations.

**Format:**
```csv
profile_id,x_ref [m],y_ref [m],z_agl [m],u [m/s],v [m/s],w [m/s],K_h [m2/s],K_v [m2/s],stability_class
profile_1,100.0,100.0,10.0,10.0,2.0,0.1,1.0,0.5,D
profile_1,100.0,100.0,50.0,12.0,2.5,0.2,2.0,1.0,D
```

**Key points:**
- Multiple profiles for different locations
- Each profile has heights in ascending order
- Wind components: u=east, v=north, w=up
- K_h usually 10-100× K_v
- Stability class: A-F (Pasquill-Gifford)

**Example:** See `docs/examples/met_profiles_spatial.csv`

#### 5. **receptors.csv** - Receptor Locations

Specify locations where concentration is sampled for output.

**Format:**
```csv
x [m],y [m],z [m],label
100.0,150.0,1.5,Receptor_1
200.0,150.0,1.5,Receptor_2
```

**Tool:** Use `receptor_grid_generator.py` to create grids automatically:
```bash
# Regular 2D grid
python receptor_grid_generator.py --grid 2d --nx 20 --ny 20 --output receptors.csv

# Radial pattern
python receptor_grid_generator.py --pattern radial --nradii 5 --ntheta 12 --output receptors_radial.csv

# Along and cross-wind transects
python receptor_grid_generator.py --pattern transect --source-x 0 --source-y 0 --output transects.csv
```

### Input File Configuration

To use these CSV files, add the following to `inputs.i`:

```
# Phase 4: CSV Input Files (all optional)
puff_model.sources_file = sources.csv
puff_model.emissions_timeseries_file = emissions_time_series.csv
puff_model.deposition_params_file = deposition_params.csv
puff_model.met_profiles_file = met_profiles.csv
puff_model.receptors_file = receptors.csv
```

**Behavior:**
- If file does not exist, defaults are applied (single source from traditional parameters)
- If file exists but is empty, defaults are applied
- All parameters are validated and reported to console
- Invalid entries are skipped with warning messages

### Example: Multi-Source Industrial Scenario

**inputs.i:**
```
# Setup basic domain
xmin = -2000.0
xmax = 10000.0
ymin = -2000.0
ymax = 10000.0
zmin = 0.0
zmax = 1000.0

# Enable puff model with multi-source CSV
puff_model.enabled = true
puff_model.sources_file = sources_industrial.csv
puff_model.emissions_timeseries_file = emissions_industrial_day.csv
puff_model.receptors_file = receptors_grid.csv

# Optional: Include spatial meteorology
puff_model.met_profiles_file = met_profiles_spatial.csv

# Output
puff_model.receptor_output_file = concentrations_receptors.csv
puff_model.output_frequency = 300.0  # Every 5 minutes
```

## Phase 4.2: Output Standardization

### Conditional Output Fields

The CSV output format automatically includes additional fields based on enabled features.

**Always included:**
```csv
x [m],y [m],z [m],concentration [units/m³]
```

**If chemistry enabled:**
```csv
...,SO2 [μg/m³],SO4 [μg/m³],NOx [μg/m³],HNO3 [μg/m³],NO3 [μg/m³]
```

**If visibility enabled:**
```csv
...,b_ext [Mm⁻¹],visual_range [km],deciview [dV]
```

**If deposition enabled:**
```csv
...,particle_deposition_flux [μg/(m² s)],deposition_mass [μg/m²]
```

**Output metadata header:**
```csv
# Concentration Output
# Generated by massconsistent_amr puff model
# Simulation time: 2024-06-15 10:30:00
# Features enabled: multi-source, time-varying emissions, chemistry, visibility
```

### Output Format Examples

**Receptor concentrations:**
```csv
# Concentration at receptor locations
receptor_id,x [m],y [m],z [m],C_total [μg/m³],SO4 [μg/m³],b_ext [Mm⁻¹],VR [km],dV
R_1,100.0,150.0,1.5,1.25,0.63,12.4,3.15,-0.08
R_2,200.0,150.0,1.5,0.89,0.44,8.8,4.45,0.94
```

**Grid concentrations:**
```csv
# 3D concentration field
x [m],y [m],z [m],C [μg/m³],particle_flux [μg/(m² s)]
100.0,100.0,12.4,1.56,0.0012
150.0,100.0,12.4,1.42,0.0011
```

## Phase 4.3: Python Preprocessing/Postprocessing Tools

### 1. **chemistry_builder.py** - Interactive Chemistry Matrix Builder

Build custom chemistry matrices with pre-built templates or interactive mode.

**Template-based (fastest):**
```bash
# SOx chemistry only
python chemistry_builder.py --template sox --output chemistry_sox.csv

# SOx + NOx
python chemistry_builder.py --template soxnox --output chemistry_soxnox.csv

# Full tropospheric chemistry
python chemistry_builder.py --template full --output chemistry_full.csv
```

**Interactive mode:**
```bash
python chemistry_builder.py --interactive
# Then follow prompts to add/edit reactions
```

**Validation:**
```bash
python chemistry_builder.py --validate chemistry.csv --verbose
```

Output: `chemistry.csv` compatible with `puff_model.chemistry_file`

### 2. **emission_profile_generator.py** - Time-Varying Emission Profiles

Generate realistic emission time series.

**24-hour traffic cycle:**
```bash
python emission_profile_generator.py --profile traffic \
  --base-rate 1.0 --peak-factor 2.0 --duration 86400 \
  --output emissions_traffic_day.csv
```

**Industrial shift operations:**
```bash
python emission_profile_generator.py --profile industrial \
  --base-rate 1.0 --duration 86400 --output emissions_industrial_24h.csv
```

**Seasonal heating variation (1 year):**
```bash
python emission_profile_generator.py --profile seasonal \
  --base-rate 1.0 --summer-factor 0.3 --duration 31536000 \
  --output emissions_yearly.csv
```

**Episodic accident (2-hour event):**
```bash
python emission_profile_generator.py --profile episodic \
  --base-rate 1.0 --event-time 43200 --event-duration 7200 \
  --event-rate 50.0 --duration 86400 --output emissions_accident.csv
```

Output: `emissions_time_series.csv` compatible with `puff_model.emissions_timeseries_file`

### 3. **receptor_grid_generator.py** - Receptor Grid Generation

Create regular or custom receptor grids.

**2D rectangular grid:**
```bash
python receptor_grid_generator.py --grid 2d --nx 30 --ny 30 \
  --x0 -1500 --y0 -1500 --dx 100 --dy 100 --z 1.5 \
  --output receptors_2d_grid.csv
```

**3D grid:**
```bash
python receptor_grid_generator.py --grid 3d --nx 20 --ny 20 --nz 4 \
  --x0 -1000 --y0 -1000 --z0 1.5 --dz 50 \
  --output receptors_3d_grid.csv
```

**Radial pattern (concentric circles):**
```bash
python receptor_grid_generator.py --pattern radial \
  --nradii 6 --ntheta 16 --rmax 5000 \
  --x-center 0 --y-center 0 --output receptors_radial.csv
```

**Along/cross-wind transects:**
```bash
python receptor_grid_generator.py --pattern transect \
  --source-x 0 --source-y 0 --wind-direction 270 \
  --output receptors_transects.csv
```

**Hazard impact zones:**
```bash
python receptor_grid_generator.py --zones impact \
  --source-x 0 --source-y 0 --wind-direction 270 \
  --red 1000 --orange 5000 --yellow 10000 \
  --output receptors_zones.csv
```

Output: `receptors.csv` compatible with `puff_model.receptors_file`

### 4. **visibility_postprocessor.py** - Visibility Impact Assessment

Post-process dispersion output to compute visibility metrics.

**Basic visibility computation:**
```bash
python visibility_postprocessor.py --input concentrations.csv \
  --output visibility_metrics.csv --species SO4,NO3
```

**With impact report:**
```bash
python visibility_postprocessor.py --input grid_concentrations.csv \
  --output visibility_grid.csv --species SO4,NO3,OC,BC,Dust \
  --baseline 200 --report impact_summary.txt
```

Output formats:
- `visibility_metrics.csv`: b_ext, visual range, deciview at each location
- `impact_summary.txt`: Statistical summary and classification

## Common Workflows

### Workflow 1: Simple Single-Source (No CSV)

```bash
# Traditional inputs.i - no CSV files needed
puff_model.enabled = true
puff_model.source_x = 100.0
puff_model.source_y = 150.0
puff_model.source_z = 50.0
puff_model.emission_rate = 1.0
```

**Still works unchanged - backward compatible!**

### Workflow 2: Multi-Source Industrial Complex

1. Create source definitions:
```bash
python src/python/receptor_grid_generator.py --grid 2d --nx 40 --ny 40 \
  --output receptors_site.csv
```

2. Prepare inputs.i:
```
puff_model.enabled = true
puff_model.sources_file = sources_plant.csv
puff_model.receptors_file = receptors_site.csv
puff_model.receptor_output_file = conc_plant.csv
```

3. Run solver:
```bash
./wind_solver inputs.i
```

4. Check results:
```bash
head -20 conc_plant.csv
```

### Workflow 3: Time-Varying Emissions with Chemistry

1. Generate time profile:
```bash
python emission_profile_generator.py --profile traffic \
  --output traffic_emissions.csv
```

2. Build chemistry matrix:
```bash
python chemistry_builder.py --template soxnox --output chemistry.csv
```

3. Configure inputs.i:
```
puff_model.enabled = true
puff_model.emissions_timeseries_file = traffic_emissions.csv
puff_model.enable_reactive_chemistry = true
puff_model.chemistry_file = chemistry.csv
```

4. Run and assess visibility:
```bash
./wind_solver inputs.i
python visibility_postprocessor.py --input receptor_concentration.csv \
  --output visibility.csv --report visibility_summary.txt
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| CSV file not found | Check file path in inputs.i; use absolute path if needed |
| CSV format error | Verify headers match documentation; no extra spaces in column names |
| No change in output | Verify CSV file is being read (check console messages); disable feature if no CSV |
| Concentrations too high/low | Check emission rates and units consistency |
| Grid has holes | Verify receptor coordinates are within domain bounds |

## References

- EPA CALPUFF Model documentation
- Turner et al. (1994): Workbook of Atmospheric Dispersion Estimates
- Seinfeld & Pandis (2016): Atmospheric Chemistry and Physics
- Zhang et al. (2001): Size-segregated particle dry deposition
- Pitchford et al. (2007): IMPROVE visibility algorithm
