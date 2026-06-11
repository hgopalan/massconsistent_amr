# Annotated: Time-Varying Meteorology (Diurnal Cycle)
# ===================================================
#
# Time-varying wind and diffusivity over diurnal cycle.
# Demonstrates realistic atmospheric dynamics with changing stability.
#
# Use case: Regulatory air quality modeling, long-range transport, episodic events

xmin = -2000.0
xmax = 20000.0
ymin = -2000.0
ymax = 20000.0
zmin = 0.0
zmax = 2000.0

n_cell_x = 220
n_cell_y = 220
n_cell_z = 35

dx = 100.0
dy = 100.0

# === METEOROLOGY VARIATION ===
# Use spatially/temporally varying meteorology profile
# (Phase 4.1 feature)

# Default (constant) wind
init_mode = loglaw
U_ref = 8.0   # Morning average
V_ref = 0.0
z_ref = 10.0
z0 = 0.2

# === PUFF MODEL ===
puff_model.enabled = true

# === CSV INPUT FILES (Phase 4.1) ===
# Most important: met_profiles_file for time-varying meteorology
puff_model.sources_file = sources_timeseries.csv
puff_model.met_profiles_file = met_profiles_diurnal.csv
puff_model.receptors_file = receptors_transport_grid.csv

# === PERSISTENT SOURCE ===
# Continuous emission throughout simulation
# File sources_timeseries.csv contains:
#   source_continuous,10000.0,10000.0,100.0,point,1.0,86400.0,1.5,10.0,350.0
# 
# This source emits 1 unit/s for entire 24-hour period

# === METEOROLOGY PROFILE (Phase 4.1) ===
# File met_profiles_diurnal.csv defines wind/diffusivity at different times
# Format:
#   time [s], height [m], U [m/s], V [m/s], K_h [m²/s], K_v [m²/s], stability_class
#
# Example entries:
#   0.0,    10.0,  2.0,  0.0,  1.0,  0.1,  E  (00:00 - stable night)
#   21600.0, 10.0, 6.0, 0.0, 10.0, 1.0, C  (06:00 - neutral morning)
#   36000.0, 10.0, 12.0, 0.0, 50.0, 5.0, A  (10:00 - unstable afternoon)
#   64800.0, 10.0, 8.0, 0.0, 20.0, 2.0, B  (18:00 - transition evening)
#   86400.0, 10.0, 3.0, 0.0, 2.0,  0.2,  F  (24:00 - stable night)

# === WIND EFFECTS ===
puff_model.enable_wind_shear = true
puff_model.wind_shear_coefficient = 0.08
puff_model.enable_veering = true
puff_model.veer_angle = 15.0

# === PLUME RISE ===
puff_model.enable_plume_rise = true
puff_model.buoyancy_coefficient = 1.2

# === DIFFUSIVITY TEMPORAL VARIATION ===
# K_h, K_v values come from met_profiles file
# But we can add min/max constraints
puff_model.K_h_min = 0.5
puff_model.K_h_max = 100.0
puff_model.K_v_min = 0.1
puff_model.K_v_max = 10.0
puff_model.enable_height_dependent_K = true
puff_model.K_profile = stability  # Choose profile based on time-varying class

# === DEPOSITION (Optional) ===
puff_model.enable_settling = true
puff_model.enable_puff_deposition = true
puff_model.deposition_params_file = deposition_params.csv

# === VISIBILITY (Optional) ===
puff_model.enable_optical_properties = false  # Disable for simplicity
puff_model.compute_visibility_at_receptors = false

# === OUTPUT ===
puff_model.receptor_output_file = transport_timeseries.csv
puff_model.grid_output_frequency = 0
puff_model.output_frequency = 300.0  # Every 5 minutes

# === SIMULATION: 24-HOUR PERIOD ===
time_stop = 86400.0
dt_base = 1.0
puff_model.dt_puff = 20.0
puff_model.n_steps_puff = 4320  # 86400 / 20

# === SETUP WORKFLOW ===
#
# 1. Create receptor grid (far-field transport domain):
#    python receptor_grid_generator.py --grid 2d \
#      --nx 40 --ny 40 --x0 5000 --y0 5000 --output receptors_transport_grid.csv
#
# 2. Create persistent source:
#    (manually create sources_timeseries.csv with single continuous source)
#    Format: source_id,x,y,z,type,rate,duration,d_stack,v_stack,T_stack
#
# 3. Create met profile with diurnal cycle (met_profiles_diurnal.csv):
#    - 00:00 (0 s): Stable (E class), low wind (2 m/s), low K
#    - 06:00 (21600 s): Neutral (C), moderate wind (6 m/s), medium K
#    - 10:00 (36000 s): Unstable (A), high wind (12 m/s), high K
#    - 18:00 (64800 s): Transition (B), moderate wind (8 m/s), medium K
#    - 24:00 (86400 s): Stable (F), low wind (3 m/s), low K
#
#    Sample file creation:
#    echo "time,height,U,V,K_h,K_v,stability" > met_profiles_diurnal.csv
#    echo "0,10,2,0,1,0.1,E" >> met_profiles_diurnal.csv
#    echo "21600,10,6,0,10,1,C" >> met_profiles_diurnal.csv
#    echo "36000,10,12,0,50,5,A" >> met_profiles_diurnal.csv
#    echo "64800,10,8,0,20,2,B" >> met_profiles_diurnal.csv
#    echo "86400,10,3,0,2,0.2,F" >> met_profiles_diurnal.csv
#
# 4. Run simulation:
#    ./wind_solver inputs_phase4_timevary.i
#
# 5. Post-process:
#    - Extract concentrations at specific receptors over time
#    - Plot time series (hourly concentrations)
#    - Identify peak concentration times

# === EXPECTED TIME SERIES BEHAVIOR ===
#
# Typical 24-hour pattern (downwind receptor):
#
# Concentration [μg/m³]
#   |     /\
#   |    /  \___
# 50|   /       \
#   |  /         \
# 40| /           \___
#   |              \
# 30|               \
#   |                \_____
# 20|                      \
#   |                       /
# 10|______________________/
#   |_____________________ time [hours]
#   0      6      12      18      24
#
# Night (00-06): Low wind + stable → HIGH concentration (limited dispersion)
# Morning (06-10): Increasing wind + unstable → DECREASING concentration (enhanced dispersion)
# Afternoon (10-18): High wind + unstable → LOW concentration (maximum dispersion)
# Evening (18-24): Decreasing wind + increasing stability → INCREASING concentration

# === KEY INSIGHTS ===
#
# 1. Stability Class Effect:
#    - Stable (E, F) → narrow plume, high concentration
#    - Unstable (A, B) → wide plume, low concentration
#
# 2. Wind Speed Effect:
#    - Low wind → concentrations increase (less dilution)
#    - High wind → concentrations decrease (more dilution)
#    - BUT: Very low wind (<1 m/s) causes slow plume transport
#
# 3. Peak Concentration Timing:
#    - Typically occurs during night/early morning (stable + low wind)
#    - Lowest during afternoon (unstable + high wind)
#
# 4. Downwind Distance Effect:
#    - Nearby receptors (1 km): Peak at night, valley at afternoon
#    - Far-field receptors (20+ km): Smoothed time series, delayed peak

# === VALIDATION APPROACH ===
#
# Compare to measurements:
# 1. Obtain hourly meteorology data (wind, temp, stability class)
# 2. Obtain hourly concentration observations
# 3. Run simulation with measured meteorology
# 4. Calculate normalized mean bias (NMB), fractional bias (FB)
# 5. Typical criteria: |NMB| < 30%, |FB| < 35%

# === VISUALIZATION ===
#
# Create time series plot:
#   - X-axis: Time (0-24 hours)
#   - Y-axis: Concentration (μg/m³)
#   - One line per downwind distance (1 km, 5 km, 10 km, 20 km)
#   - Overlay measured concentrations (if available)
#
# Create wind rose:
#   - Show wind direction/speed variation over day
#   - Annotate with stability class
#
# Create Hovmöller diagram:
#   - X-axis: Time, Y-axis: Downwind distance
#   - Color: Concentration
#   - Shows plume propagation and time evolution

# === TROUBLESHOOTING ===
#
# Time series shows no variability:
# - Check met_profiles_diurnal.csv is being read
# - Verify time points cover entire simulation
# - Interpolation: values between time points should be linearly interpolated
#
# Peak concentration at wrong time:
# - Verify wind direction in met_profiles (check V component)
# - Check stability class transitions (may need intermediate times)
# - Validate source position relative to receptor
#
# Unrealistic concentrations:
# - Check emission rate (units consistent with output)
# - Check K values reasonable for stability class
# - Verify source height > domain zmin

# === REFERENCES ===
# - Stability classification: Pasquill-Gifford (Turner 1994)
# - Diurnal boundary layer: Stull (1988)
# - Gaussian plume model: Beychok (2005)
# - Time-varying dispersion: EPA guidelines
# - Regional transport models: Carmichael et al. (1991)
