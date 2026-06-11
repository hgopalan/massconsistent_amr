# Sky View Factor Test: Buildings on Terrain
# Tests: SVF computation with buildings on complex terrain
# Features: Unified terrain+building SVF with terrain-building interactions
# Domain: Gaussian hill with buildings at different elevations
# Expected: SVF accounts for both terrain slope AND building geometry

terrain_file = terrain.csv
building_file = buildings.csv

# Reference wind
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 25.0

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
plot_file = plt_sky_view_factor_buildings_terrain
