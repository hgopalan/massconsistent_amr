# Urban Layout Test
# Tests: Regular box, L-shaped, T-shaped, U-shaped and polygonal buildings on flat terrain
# Verifies that solver converges with multiple complex building geometries

# Terrain file
terrain_file = terrain.csv

# Reference wind: 10 m/s from West at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m]
dx = 10.0
dy = 10.0
dz = 10.0

# Domain height [m] above maximum obstacle elevation
domain_height = 100.0

# Buildings from CSV file
building_file = buildings.csv

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 1
max_grid_size = 32

# Output plotfile
plot_file = plt_urban_layout
