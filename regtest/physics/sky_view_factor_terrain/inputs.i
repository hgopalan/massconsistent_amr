# Sky View Factor Test: Terrain Only
# Tests: Computation of sky view factor from combined terrain+building elevation
# Features: SVF from topography gradient (simple slope-based formula)
# Domain: Flat with Gaussian hill for testing slope variation
# Expected output: SVF field (0-1) corresponding to local terrain slope

terrain_file = terrain.csv

# Reference wind
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 30.0

# Domain height [m]
domain_height = 150.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Sky View Factor Computation
enable_sky_view_factor = true
latitude_degrees = 40.0
day_of_year = 172.0
hour_of_day = 12.0
max_horizon_distance = 500.0

# MLMG solver settings
mlmg_verbose = 0
max_grid_size = 32

# Output plotfile
plot_file = plt_sky_view_factor_terrain
