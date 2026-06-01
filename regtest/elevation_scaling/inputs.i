# Elevation-Dependent Wind Speed Scaling Test
# Tests: Terrain elevation effects on wind speed (Feature 6)
# Mountain-valley flow with wind reduction at elevation

# Terrain file (Gaussian hill to create elevation variation)
terrain_file = terrain.csv

# Log-law initialization with elevation scaling
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Feature 6: Enable elevation-dependent wind scaling
enable_elevation_scaling = true
elevation_scaling_factor = 0.3    # Positive: wind decreases with elevation
elevation_height_scale = 1000.0   # Characteristic height scale [m]

# Grid spacing [m]
dx = 30.0
dy = 30.0
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
plot_file = plt_elevation_scaling
