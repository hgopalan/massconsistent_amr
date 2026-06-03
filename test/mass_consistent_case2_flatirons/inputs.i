# Case 2: Flatirons NREL Site with Time-Varying Winds and Turbulence
# Tests mass-consistent wind solver with real-world SRTM terrain
# Terrain: Flatirons region near Boulder, CO (NREL test site)

# Terrain file (user-generated from SRTM data)
terrain_file = terrain.csv

# Reference wind: 12 m/s from northwest at 10 m AGL
U_ref = 12.0
V_ref = -3.0
z_ref = 10.0

# Aerodynamic roughness length [m] (mixed grass and forest)
z0 = 0.1

# Horizontal grid spacing [m]
dx = 50.0
dy = 50.0

# Vertical grid spacing [m] for adequate resolution
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 250.0

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
plot_file = plt_case2_output
