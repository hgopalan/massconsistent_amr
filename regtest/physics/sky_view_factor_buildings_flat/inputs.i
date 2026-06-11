# Sky View Factor Test: Buildings on Flat Terrain
# Tests: SVF computation with buildings on flat domain
# Features: Unified terrain+building SVF (buildings treated as elevation features)
# Domain: Flat (z=0), 200m x 200m, with 3 buildings

terrain_file = terrain.csv
building_file = buildings.csv

# Reference wind
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 10.0
dy = 10.0
dz = 10.0

# Domain height [m]
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Sky View Factor Computation (unified terrain+building approach)
enable_sky_view_factor = true
enable_solar_shading = true
latitude_degrees = 40.0
day_of_year = 172.0
hour_of_day = 12.0
max_horizon_distance = 500.0

# MLMG solver settings
mlmg_verbose = 0
max_grid_size = 32

# Output plotfile
plot_file = plt_sky_view_factor_buildings_flat
