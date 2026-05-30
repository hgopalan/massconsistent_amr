# Buildings on Flat Terrain Test
# Tests: buildings as union of boxes on flat terrain
# Verifies that building masking works correctly and solver converges

# Terrain file (flat surface at z=0, 0-200m domain)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m] -- gives reasonable resolution for buildings
dx = 10.0
dy = 10.0
dz = 10.0

# Domain height [m] above maximum obstacle elevation
domain_height = 100.0

# Buildings from CSV file
# Building 1: 40-60m x 40-60m, height 0-30m
# Building 2: 100-140m x 60-80m, height 0-50m
building_file = buildings.csv

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 1
max_grid_size = 32

# Output plotfile
plot_file = plt_buildings_flat
