# Buildings on Gaussian Hill Terrain Test
# Tests: buildings sitting on terrain (terrain-aligned)
# Verifies that building heights are adjusted to start from local terrain elevation
# Terrain: 11x11 grid Gaussian hill, peak 50 m at center
# Buildings: 3 buildings at different positions on the hill

# Terrain file (Gaussian hill: 0-300m x 0-300m domain, peak 50m at center)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (open terrain / short grass)
z0 = 0.03

# Horizontal grid spacing [m] (matches terrain point spacing)
dx = 30.0
dy = 30.0

# Vertical grid spacing [m]
dz = 25.0

# Domain height [m] above maximum obstacle elevation
domain_height = 100.0

# Buildings on the hill
# zmin=0 means buildings sit on terrain; height = zmax - zmin = 30 m
building_file = buildings.csv

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 1
max_grid_size = 32

# Output plotfile
plot_file = plt_buildings_terrain
