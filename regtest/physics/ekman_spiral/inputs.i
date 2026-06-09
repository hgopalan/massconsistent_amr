# Analytical Ekman Spiral vertical profile initialization Test
# Tests: init_mode = ekman_spiral using classical Ekman equations

# Terrain file
terrain_file = terrain.csv

# Analytical Ekman Spiral parameters
init_mode = ekman_spiral
ekman_latitude = 45.0
ekman_ug = 10.0
ekman_vg = 0.0
ekman_Km = 5.0

# Reference height parameters (unused but kept for boilerplate safety)
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 10.0

# Domain height [m]
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at multiple heights to see veer profile
extract_agl  = 10.0 50.0 100.0 200.0
extract_file = ekman_spiral_extract.csv

# Output plotfile
plot_file = plt_ekman_spiral
