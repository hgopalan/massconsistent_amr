# Case 1: Gaussian Hill with Time-Varying Winds and Turbulence
# Tests mass-consistent wind solver with medium Gaussian hill terrain
# Terrain: 21x21 grid over a 500x500 m domain, peak elevation 75 m at centre

# Terrain file (pre-generated 21x21 Gaussian hill)
terrain_file = terrain.csv

# Reference wind: 15 m/s from west at 10 m AGL
U_ref = 15.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (open terrain / short grass)
z0 = 0.03

# Horizontal grid spacing [m] (matches terrain point spacing from terrain_gen.py)
dx = 25.0
dy = 25.0

# Vertical grid spacing [m] for adequate resolution
dz = 20.0

# Domain height [m] above maximum terrain elevation
domain_height = 200.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (minimal output)
mlmg_verbose = 0
max_grid_size = 32

# Extract wind at 30 m AGL and write to CSV
extract_agl = 30.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_case1_output
