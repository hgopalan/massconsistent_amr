# Colorado Complex Terrain Drone Spray - Inputs Configuration
terrain_file = terrain.csv
landuse_file = landuse.csv

# Grid spacing (matched with 21x21 grid)
dx = 25.6941744
dy = 27.7500000
dz = 10.0

# 20 levels in vertical direction
domain_height = 200.0

# Mass-consistent coefficients (slight anisotropy preferencing horizontal flow)
alpha_h = 1.2
alpha_v = 0.8

# Solver settings
mlmg_verbose = 0
max_grid_size = 32

# Wind profile setup (Log-law with reference wind)
init_mode = loglaw
U_ref = 7.5
V_ref = 6.0
z_ref = 10.0
z0 = 0.1

# Enable land-use classification
enable_landuse_classification = true
landuse_interp_method = idw

# Output plotfile prefix
plot_file = plt_colorado_drone
