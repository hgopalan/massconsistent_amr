# Morphometric Models Test
# Tests: Macdonald, Kutzbach, and Bottema morphometric algorithms for displacement height and roughness length.

# Terrain file
terrain_file = terrain.csv

# Building file
building_file = buildings.csv

# Reference wind: 10 m/s from west at 50 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 50.0

# Base aerodynamic roughness length [m] for ground surface
z0 = 0.05

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 5.0

# Domain height [m]
domain_height = 200.0

# Morphometric model parameters
enable_morphometric_models = true
morphometric_model_type = macdonald

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32

# Output plotfile
plot_file = plt_morphometric
