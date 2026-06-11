# Example Phase 4: Multi-Source Industrial Complex
# ================================================
#
# This example demonstrates Phase 4 capabilities:
# - Multiple emission sources with different stack parameters
# - Time-varying emission rates (daily cycle)
# - Spatial meteorology profiles
# - Chemistry-enabled dispersion
# - Receptor grid output

# === DOMAIN SETUP ===
xmin = -2000.0
xmax = 15000.0
ymin = -2000.0
ymax = 15000.0
zmin = 0.0
zmax = 1500.0

# Number of grid cells
n_cell_x = 170
n_cell_y = 170
n_cell_z = 30

# Grid spacing
dx = 100.0
dy = 100.0

# === INITIAL CONDITIONS & WIND ===
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.5

# === TERRAIN (Optional) ===
# terrain_file = terrain.csv

# === PUFF DISPERSION MODEL ===
puff_model.enabled = true

# Phase 4.1: CSV Input Infrastructure
# ====================================

# Multi-source definitions
puff_model.sources_file = sources_industrial.csv

# Time-varying emission rates
puff_model.emissions_timeseries_file = emissions_daily.csv

# Spatial meteorology profiles
puff_model.met_profiles_file = met_profiles_spatial.csv

# Receptor locations
puff_model.receptors_file = receptors_industrial_grid.csv

# Phase 3 Features (already available)
# ====================================

# Plume rise for elevated sources
puff_model.enable_plume_rise = true

# Wind shear with height
puff_model.enable_wind_shear = true
puff_model.wind_shear_coefficient = 0.05
puff_model.veer_angle = 15.0

# Terrain reflection (if terrain_file specified)
puff_model.enable_terrain_reflection = false
puff_model.use_image_source = true

# Chemistry
puff_model.enable_reactive_chemistry = true
puff_model.chemistry_file = chemistry_soxnox.csv
puff_model.chemistry_timestep = 1.0
puff_model.enable_temperature_dependent_rates = true
puff_model.enable_rh_dependent_rates = true

# Deposition
puff_model.enable_settling = true
puff_model.enable_puff_deposition = true
puff_model.enable_wet_deposition = false  # No rain in this scenario
puff_model.deposition_params_file = deposition_params.csv

# Visibility
puff_model.enable_optical_properties = true
puff_model.compute_visibility_at_receptors = true

# === DIFFUSIVITY ===
puff_model.K_h = 10.0
puff_model.K_v = 2.0
puff_model.enable_height_dependent_K = true
puff_model.K_profile = stability

# === OUTPUT ===
puff_model.receptor_output_file = industrial_concentrations.csv
puff_model.grid_output_frequency = 0  # Disable grid output (just receptors)
puff_model.output_frequency = 300.0  # Every 5 minutes

# === SIMULATION TIMING ===
# Simulate 24-hour period
time_stop = 86400.0
dt_base = 1.0
puff_model.dt_puff = 10.0
puff_model.n_steps_puff = 8640  # One per 10 seconds

# === NOTES ===
# Prepare input files before running:
#
# 1. Generate receptor grid:
#    python src/python/receptor_grid_generator.py --grid 2d \
#      --nx 30 --ny 30 --x0 -1500 --y0 -1500 --output receptors_industrial_grid.csv
#
# 2. Create emissions profile:
#    python src/python/emission_profile_generator.py --profile traffic \
#      --base-rate 1.0 --peak-factor 2.5 --output emissions_daily.csv
#
# 3. Build chemistry matrix:
#    python src/python/chemistry_builder.py --template soxnox \
#      --output chemistry_soxnox.csv
#
# 4. Prepare multi-source CSV (sources_industrial.csv) with entries like:
#    source_1,100.0,150.0,50.0,point,1.5,86400.0,1.5,12.0,350.0
#    source_2,500.0,150.0,30.0,point,0.8,86400.0,1.0,8.0,310.0
#
# 5. Prepare meteorology profiles CSV (met_profiles_spatial.csv)
#
# Run with:
#    ./wind_solver inputs_phase4_industrial.i
#
# Post-process results:
#    python src/python/visibility_postprocessor.py \
#      --input industrial_concentrations.csv \
#      --output visibility_industrial.csv \
#      --species SO4,NO3 \
#      --report impact_summary.txt
