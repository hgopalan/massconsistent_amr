# Building Porosity Model Test
# Tests: Porous flow through structures 
# Tree stand simulation with partial flow-through

# Terrain file (flat terrain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Enable building porosity model
enable_building_porosity = true
building_porosity_file = porous_buildings.csv
porosity_drag_coefficient = 0.3

# Grid spacing [m]
dx = 10.0
dy = 10.0
dz = 10.0

# Domain height [m] above maximum terrain elevation
domain_height = 50.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 10 m AGL
extract_agl  = 10.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_porous_buildings
