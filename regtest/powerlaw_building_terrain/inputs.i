# Power-Law Profile Wind Solver Test (Terrain + Building)
# Tests: power-law wind initialization (Phase 1 Feature 1) with buildings on complex terrain
# Combines terrain-following, buildings, and power-law profile

# Terrain file (sloped domain)
terrain_file = terrain.csv

# Building file
building_file = buildings.csv

# Power-law initialization mode (Phase 1 Feature 1)
init_mode = powerlaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
powerlaw_exponent = 0.143  # ~1/7 typical for neutral conditions

# Grid spacing [m]
dx = 10.0
dy = 10.0
dz = 10.0

# Domain height [m] above maximum terrain elevation
domain_height = 200.0

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
plot_file = plt_powerlaw_building_terrain
