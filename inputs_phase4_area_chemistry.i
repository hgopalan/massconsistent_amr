# Annotated: Area Source with Reactive Chemistry
# ===============================================
#
# Area source (e.g., agricultural field, urban neighborhood) with 
# SO₂ → SO₄ chemistry. Demonstrates Phase 4 capabilities.
#
# Use case: Agricultural burning, urban air quality, permit modeling

xmin = -3000.0
xmax = 15000.0
ymin = -3000.0
ymax = 15000.0
zmin = 0.0
zmax = 1500.0

n_cell_x = 180
n_cell_y = 180
n_cell_z = 30

dx = 100.0
dy = 100.0

# === WIND WITH SHEAR ===
init_mode = loglaw
U_ref = 8.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.3  # Moderate roughness

# === PUFF MODEL ===
puff_model.enabled = true

# === CSV INPUT FILES (Phase 4) ===
puff_model.sources_file = sources_area.csv
puff_model.emissions_timeseries_file = emissions_diurnal.csv
puff_model.receptors_file = receptors_area_grid.csv

# === AREA SOURCE DEFINITION (Phase 4.1) ===
# File sources_area.csv should contain:
#   source_id,x [m],y [m],z [m],type,emission_rate [units/s],emission_duration [s],...
#   area_field,5000.0,5000.0,0.0,area,10.0,14400.0,500.0,500.0
#
# Parameters for area source:
#   x, y: center location
#   z: release height (0 = ground level)
#   type: "area"
#   emission_rate: units/s emitted from area
#   length_x, length_y: dimensions in meters

# === CHEMISTRY ACTIVATION (Phase 3) ===
puff_model.enable_reactive_chemistry = true
puff_model.chemistry_file = chemistry_soxnox.csv
puff_model.chemistry_timestep = 1.0

# Temperature-dependent reaction rates
puff_model.enable_temperature_dependent_rates = true

# RH-dependent rates (important for SO₂ oxidation)
puff_model.enable_rh_dependent_rates = true

# === DEPOSITION (Phase 3) ===
puff_model.enable_settling = true
puff_model.enable_puff_deposition = true
puff_model.deposition_params_file = deposition_params.csv

# === VISIBILITY (Phase 3) ===
puff_model.enable_optical_properties = true
puff_model.compute_visibility_at_receptors = true

# === DIFFUSIVITY ===
puff_model.K_h = 20.0  # Higher K for unstable conditions over heated surface
puff_model.K_v = 5.0
puff_model.enable_height_dependent_K = true
puff_model.K_profile = stability

# === WIND EFFECTS ===
puff_model.enable_wind_shear = true
puff_model.wind_shear_coefficient = 0.05
puff_model.veer_angle = 10.0

# === OUTPUT ===
puff_model.receptor_output_file = area_source_results.csv
puff_model.grid_output_frequency = 0
puff_model.output_frequency = 300.0  # Every 5 minutes

# === SIMULATION ===
# Simulate 4-hour period (typical burn day)
time_stop = 14400.0
dt_base = 1.0
puff_model.dt_puff = 10.0
puff_model.n_steps_puff = 1440

# === SETUP WORKFLOW ===
#
# 1. Create receptor grid (2km × 2km, 100m spacing):
#    python receptor_grid_generator.py --grid 2d \
#      --nx 20 --ny 20 --x0 3000 --y0 3000 --output receptors_area_grid.csv
#
# 2. Create time-varying emissions (2x nighttime, peak at noon):
#    python emission_profile_generator.py --profile agricultural \
#      --base-rate 1.0 --peak-factor 2.0 --duration 14400 --output emissions_diurnal.csv
#
# 3. Create chemistry matrix (SOₓ only):
#    python chemistry_builder.py --template sox --output chemistry_soxnox.csv
#
# 4. Create area source CSV (500m × 500m field centered at 5000, 5000):
#    (manually create sources_area.csv, or extend existing sources.csv)
#
# 5. Run simulation:
#    ./wind_solver inputs_phase4_area_chemistry.i
#
# 6. Post-process for visibility:
#    python visibility_postprocessor.py \
#      --input area_source_results.csv \
#      --output visibility_area.csv \
#      --species SO2,SO4 \
#      --report area_impact.txt

# === EXPECTED RESULTS ===
# - Concentrations peak downwind of area source
# - SO₂ decreases with distance (oxidized to SO₄)
# - SO₄ increases downwind (secondary formation)
# - Visibility degrades proportional to SO₄ + OC
# - Deposition removes particles >50 km

# === NEXT STEPS ===
# - Add time-varying meteorology: puff_model.met_profiles_file
# - Add terrain effects: terrain_file
# - Add multiple area sources: expand sources_area.csv
# - Validate against measurements (if available)

# === REFERENCES ===
# - EPA CALPUFF: Scire et al. (2000)
# - Agricultural burning SO₂: Reid et al. (2005)
# - Gaussian plume model: Turner (1994)
