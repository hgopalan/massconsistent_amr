# Example Input Files for Phase 4

This directory contains annotated example input files demonstrating Phase 4 capabilities.

## Files

### **inputs_phase4_industrial.i** - Multi-Source Industrial Complex

**Scenario:** Industrial facility with multiple elevated stacks, time-varying emissions, chemistry, and deposition.

**Demonstrates:**
- Multi-source emission definitions (sources_industrial.csv)
- Time-varying emission rates (daily traffic-like cycle)
- Spatial meteorology profiles
- Chemistry-enabled dispersion (SO₂ oxidation to SO₄, NOₓ → HNO₃)
- Deposition modeling (dry deposition of particles)
- Visibility impact assessment
- Receptor grid output

**Use Case:**
- Industrial permit modeling
- EPA CALPUFF equivalence
- Regulatory air quality compliance
- Visibility impact on parks/scenic areas

**Workflow:**

1. **Generate input files:**
   ```bash
   # Receptor grid (30×30 points, 100m spacing, centered on source)
   python src/python/receptor_grid_generator.py --grid 2d \
     --nx 30 --ny 30 --x0 -1500 --y0 -1500 \
     --output receptors_industrial_grid.csv
   
   # Time-varying emissions (traffic-like pattern over 24 hours)
   python src/python/emission_profile_generator.py --profile traffic \
     --base-rate 1.0 --peak-factor 2.5 --duration 86400 \
     --output emissions_daily.csv
   
   # Chemistry matrix (SOₓ + NOₓ)
   python src/python/chemistry_builder.py --template soxnox \
     --output chemistry_soxnox.csv
   ```

2. **Prepare multi-source CSV (sources_industrial.csv):**
   ```csv
   source_id,x [m],y [m],z [m],type,emission_rate [units/s],emission_duration [s],stack_diameter [m],stack_exit_velocity [m/s],stack_exit_temperature [K]
   stack_1,100.0,150.0,80.0,point,1.5,86400.0,1.5,12.0,350.0
   stack_2,500.0,150.0,50.0,point,0.8,86400.0,1.0,8.0,310.0
   ```

3. **Run solver:**
   ```bash
   ./wind_solver inputs_phase4_industrial.i
   ```

4. **Post-process for visibility:**
   ```bash
   python src/python/visibility_postprocessor.py \
     --input industrial_concentrations.csv \
     --output visibility_industrial.csv \
     --species SO4,NO3,OC,BC \
     --baseline 200 \
     --report impact_summary.txt
   ```

5. **Check results:**
   ```bash
   # View first few receptors
   head -10 industrial_concentrations.csv
   
   # View visibility summary
   cat impact_summary.txt
   ```

**Expected Output:**
- `industrial_concentrations.csv` - Concentrations at 900 receptors (SO₂, SO₄, NOₓ, HNO₃, NO₃⁻)
- `visibility_industrial.csv` - Visibility metrics (extinction, visual range, deciview)
- `impact_summary.txt` - Statistical summary and classification

**Key Insights:**
- Multiple sources create overlapping plumes
- Time-varying emissions produce daily cycles in concentration
- Chemistry transforms species downwind (SO₂→SO₄, NOₓ→HNO₃)
- Deposition reduces particle concentrations >5-10 km downwind
- Visibility degradation follows aerosol concentration patterns

---

### **Customization Guide**

#### Change Domain Size
```ini
# Larger domain (±10 km):
xmin = -10000.0
xmax = 50000.0
ymin = -10000.0
ymax = 50000.0
```

#### Add Terrain Effects
```ini
# Include terrain file
terrain_file = terrain_complex.csv
puff_model.enable_terrain_reflection = true
```

#### Include Deposition
```ini
# Reference custom deposition parameters
puff_model.deposition_params_file = deposition_custom.csv
```

#### Adjust Meteorology
```ini
# Stronger wind
U_ref = 15.0

# More stable conditions
z0 = 1.0  # Rougher surface → more stable

# Include spatial variations
puff_model.met_profiles_file = met_complex.csv
```

#### Run Longer Simulation
```ini
# 7-day simulation
time_stop = 604800.0

# Update puff model steps
puff_model.n_steps_puff = 60480  # 604800 / 10
```

---

## Related Documentation

- `docs/PHASE4_CSV_INFRASTRUCTURE.md` - Comprehensive CSV infrastructure guide
- `docs/FEATURE_MIGRATION_GUIDE.md` - Step-by-step feature progression
- `src/python/README_PREPROCESSING.md` - Python tools guide
- `docs/PHASE3_ENHANCEMENTS.md` - Phase 3 features (chemistry, deposition, visibility)

## Testing Checklist

- [ ] CSV files load successfully (check console output)
- [ ] Concentrations reasonable magnitude (1-100 μg/m³)
- [ ] Multiple source peaks visible downwind
- [ ] Chemistry transforms species over time
- [ ] Deposition reduces concentrations >10 km
- [ ] Visibility metrics correlate with concentrations
- [ ] Performance <1 second per output step
