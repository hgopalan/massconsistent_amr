# Python Preprocessing & Postprocessing Tools

This directory contains utility scripts for preparing inputs and post-processing outputs for the massconsistent_amr puff model.

## Quick Start

All scripts have built-in help:

```bash
python chemistry_builder.py --help
python emission_profile_generator.py --help
python receptor_grid_generator.py --help
python visibility_postprocessor.py --help
```

## Scripts Overview

### 1. **chemistry_builder.py** - Build Chemistry Matrices

Create chemical reaction networks for multi-species reactive transport.

**Key Features:**
- Pre-built templates (SOx, NOx, full tropospheric)
- Interactive mode for custom configurations
- Validation against atmospheric chemistry literature
- CSV export for puff model

**Quick Examples:**
```bash
# Template-based (fastest)
python chemistry_builder.py --template soxnox --output chemistry.csv

# Interactive mode
python chemistry_builder.py --interactive

# Validate existing matrix
python chemistry_builder.py --validate chemistry.csv --verbose
```

**Output:** `chemistry.csv` compatible with `puff_model.chemistry_file` in inputs.i

**Common use cases:**
- Industrial emission with SO₂ oxidation
- Urban air quality with NOx chemistry
- Full regional chemistry network

---

### 2. **emission_profile_generator.py** - Generate Time-Varying Emissions

Create realistic emission time series for various scenarios.

**Key Features:**
- Multiple profile templates (traffic, industrial, residential, seasonal)
- Customizable peak factors and timing
- Support for episodic events
- CSV export

**Quick Examples:**
```bash
# 24-hour traffic cycle (peak 2x baseline at rush hours)
python emission_profile_generator.py --profile traffic \
  --base-rate 1.0 --duration 86400 --output emissions_traffic.csv

# Industrial shift operations
python emission_profile_generator.py --profile industrial \
  --output emissions_industrial_24h.csv

# Weekly variation (weekends at 70% of weekday)
python emission_profile_generator.py --profile weekly \
  --weekend-factor 0.7 --duration 604800 --output emissions_week.csv

# Seasonal heating (summer at 30% of winter)
python emission_profile_generator.py --profile seasonal \
  --summer-factor 0.3 --duration 31536000 --output emissions_year.csv

# Episodic accident (10x baseline for 2 hours starting at t=12000s)
python emission_profile_generator.py --profile episodic \
  --base-rate 1.0 --event-time 43200 --event-duration 7200 \
  --event-rate 10.0 --output emissions_accident.csv
```

**Output:** `emissions_time_series.csv` compatible with `puff_model.emissions_timeseries_file`

**Common use cases:**
- Traffic emissions (morning/evening peaks)
- Industrial operations (shift changes)
- Building heating (seasonal)
- Accident/emergency scenarios

---

### 3. **receptor_grid_generator.py** - Generate Receptor Grids

Create receptor locations for sampling concentration output.

**Key Features:**
- Regular 2D/3D grids
- Radial patterns (concentric circles)
- Along-wind and cross-wind transects
- Impact zone boundaries
- CSV export

**Quick Examples:**
```bash
# 2D rectangular grid (20×20 points, 100m spacing)
python receptor_grid_generator.py --grid 2d --nx 20 --ny 20 \
  --x0 -1000 --y0 -1000 --dx 100 --dy 100 --output receptors_2d.csv

# 3D grid (for vertical concentration profiles)
python receptor_grid_generator.py --grid 3d --nx 20 --ny 20 --nz 5 \
  --x0 -1000 --y0 -1000 --z0 1.5 --dz 100 --output receptors_3d.csv

# Radial pattern from source
python receptor_grid_generator.py --pattern radial \
  --nradii 6 --ntheta 16 --rmax 5000 --output receptors_radial.csv

# Along and cross-wind transects
python receptor_grid_generator.py --pattern transect \
  --source-x 0 --source-y 0 --wind-direction 270 --output receptors_transects.csv

# Impact zone boundaries (red/orange/yellow zones)
python receptor_grid_generator.py --zones impact \
  --source-x 0 --source-y 0 --red 1000 --orange 5000 --yellow 10000 \
  --output receptors_zones.csv
```

**Output:** `receptors.csv` compatible with `puff_model.receptors_file`

**Common use cases:**
- Ground-level impact assessment (2D grid at z=1.5m)
- Vertical profiling (3D grid)
- Source-centered analysis (radial pattern)
- Regulatory modeling (impact zones)

---

### 4. **visibility_postprocessor.py** - Compute Visibility Metrics

Post-process concentration output to calculate visibility impacts.

**Key Features:**
- IMPROVE algorithm for extinction coefficient
- Visual range (Koschmieder equation)
- Deciview scale
- Fog and icing probability
- Summary report generation

**Quick Examples:**
```bash
# Basic visibility computation
python visibility_postprocessor.py --input receptor_concentrations.csv \
  --output visibility_metrics.csv --species SO4,NO3

# With impact summary report
python visibility_postprocessor.py --input grid_concentrations.csv \
  --output visibility_grid.csv --species SO4,NO3,OC,BC,Dust \
  --baseline 200 --report impact_summary.txt

# Verbose mode (shows statistics)
python visibility_postprocessor.py --input concentrations.csv \
  --output visibility.csv --verbose --report report.txt
```

**Output Files:**
- `visibility_metrics.csv`: b_ext, visual range, deciview at each receptor
- `impact_summary.txt`: Statistical summary and classification

**Common use cases:**
- Regulatory visibility compliance (Regional Haze Rule)
- Air quality impact assessment
- Park/scenic area protection
- Aviation safety (visibility thresholds)

---

## Integration with Puff Model

### Typical Workflow

1. **Generate inputs:**
   ```bash
   python receptor_grid_generator.py --grid 2d --output receptors.csv
   python chemistry_builder.py --template soxnox --output chemistry.csv
   python emission_profile_generator.py --profile traffic --output emissions.csv
   ```

2. **Configure inputs.i:**
   ```ini
   puff_model.receptors_file = receptors.csv
   puff_model.chemistry_file = chemistry.csv
   puff_model.emissions_timeseries_file = emissions.csv
   ```

3. **Run solver:**
   ```bash
   ./wind_solver inputs.i
   ```

4. **Post-process results:**
   ```bash
   python visibility_postprocessor.py --input receptor_concentration.csv \
     --output visibility.csv --report summary.txt
   ```

---

## CSV File Formats

### chemistry.csv
```csv
reaction_id,reaction_type,reactants,products,rate_constant [1/s],temp_coeff [1/K],rh_coeff [1/%]
r1,oxidation,SO2,SO4,0.001,0.04,-0.005
```

### emissions_time_series.csv
```csv
time [s],emission_rate [units/s]
0.0,1.0
3600.0,1.5
7200.0,1.0
```

### receptors.csv
```csv
x [m],y [m],z [m],label
100.0,150.0,1.5,R_1
200.0,150.0,1.5,R_2
```

### visibility_metrics.csv
```csv
x [m],y [m],z [m],C [μg/m³],b_ext [Mm⁻¹],VR [km],dV
100.0,150.0,1.5,1.25,12.4,3.15,-0.08
```

---

## Troubleshooting

### Q: "Module not found" when running scripts
**A:** Ensure you're running from the `src/python` directory:
```bash
cd src/python
python chemistry_builder.py --help
```

### Q: CSV file has wrong format
**A:** Check:
- No extra spaces in column headers
- ASCII encoding (not UTF-8 BOM)
- Line endings are LF (not CRLF on Windows)

### Q: Solver doesn't load CSV file
**A:** Verify:
- Filename matches exactly in inputs.i
- File exists in solver's working directory
- Check console output for error messages

### Q: Generated receptor grid has wrong bounds
**A:** Use `--x0` and `--y0` to set grid origin:
```bash
python receptor_grid_generator.py --grid 2d \
  --x0 -1500 --y0 -1500 --dx 100 --output receptors.csv
```

### Q: Chemistry matrix validation fails
**A:** Check:
- Rate constants are positive
- No duplicate reaction IDs
- Reactants and products are defined
- Rate constants in reasonable range (1e-5 to 0.1 s⁻¹)

---

## Performance Notes

- **chemistry_builder.py**: Interactive <1 s, template modes instant
- **emission_profile_generator.py**: ~1 s for 1-year profile
- **receptor_grid_generator.py**: Instant for grids <100k points
- **visibility_postprocessor.py**: ~5 s for 10k concentrations

---

## References

- EPA CALPUFF Model Documentation
- IMPROVE Algorithm (Pitchford et al., 2007)
- Turner et al. (1994): Workbook of Atmospheric Dispersion Estimates
- Seinfeld & Pandis (2016): Atmospheric Chemistry and Physics

---

## See Also

- `docs/PHASE4_CSV_INFRASTRUCTURE.md` - Comprehensive CSV infrastructure guide
- `docs/FEATURE_MIGRATION_GUIDE.md` - Step-by-step feature progression
- `inputs_single.i` - Example single-source configuration
- `inputs_multi.i` - Example multi-source configuration
