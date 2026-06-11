# Flatirons Campus with Random Buildings - Sky View Factor Test
# Tests unified SVF computation with buildings on real-world terrain
# Features: Terrain-building interactions, solar shading, urban canyon effects

# Note: Run test_flatirons_buildings_svf.py first to generate terrain.csv and buildings.csv
terrain_file = terrain.csv
building_file = buildings.csv

# Reference wind
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 20.0
dy = 20.0
dz = 20.0

# Domain height [m]
domain_height = 200.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Sky View Factor and Solar Shading (unified terrain+building approach)
enable_sky_view_factor = true
enable_solar_shading = true
latitude_degrees = 40.015   # Flatirons latitude
longitude_degrees = -105.241  # Flatirons longitude
day_of_year = 172.0          # June 21 (summer solstice)
hour_of_day = 12.0           # Noon
max_horizon_distance = 1000.0

# Buoyancy stratification with diurnal cycle
enable_buoyancy_stratification = true
enable_diurnal_temperature = true
diurnal_temperature_amplitude = 8.0  # +/- 8K
diurnal_phase_hour = 14.0            # Peak heating at 2 PM
buoyancy_coefficient = 1.0

# MLMG solver settings
mlmg_verbose = 1
max_grid_size = 32
tol_rel = 1.e-8

# Output
plot_file = plt_flatirons_buildings_svf
