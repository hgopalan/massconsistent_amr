# Time-Varying Wind Boundary Conditions Test
# Tests: Time-dependent inflow conditions (Feature 7)
# Note: Current implementation uses first time point

# Terrain file (flat terrain for clarity)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
z_ref = 10.0
z0 = 0.1

# Feature 7: Enable time-varying boundary conditions
enable_time_varying = true
time_series_file = time_series.csv

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_time_varying
