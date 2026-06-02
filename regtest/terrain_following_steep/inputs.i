# Terrain-Following Coordinates Test on Steep Hill
# Tests: terrain-following (streamline) coordinate transformation on steep terrain
# Terrain: Steep Gaussian hill with 45-degree slope at base
# Purpose: Verify improved mass consistency on steep slopes with terrain-following coords

# Terrain file (pre-generated steep Gaussian hill)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (open terrain / short grass)
z0 = 0.03

# Horizontal grid spacing [m]
dx = 20.0
dy = 20.0

# Vertical grid spacing [m]
dz = 15.0

# Domain height [m] above maximum terrain elevation
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable terrain-following coordinates
enable_terrain_following = true
# Decay height (auto-set to domain_height / 3 = 100 m if not specified)
# terrain_decay_height = 100.0

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32
tol_rel = 1.e-8

# Extract wind at 15 m AGL and write to CSV
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_terrain_following_steep
