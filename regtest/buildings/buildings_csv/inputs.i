# Buildings CSV Format Test
# Tests: reading buildings from CSV file with xmin xmax ymin ymax zmin zmax format
# Verifies that building file reading works correctly with three buildings

# Terrain file (flat surface at z=0, 0-200m domain)
terrain_file = terrain.csv

# Buildings from CSV file
building_file = buildings.csv

# Reference wind: 12 m/s from southwest at 10 m AGL
U_ref = 8.5
V_ref = 8.5
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m] -- reasonable resolution for buildings
dx = 10.0
dy = 10.0
dz = 10.0

# Domain height [m] above maximum obstacle elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 1
max_grid_size = 32

# Output plotfile
plot_file = plt_buildings_csv
