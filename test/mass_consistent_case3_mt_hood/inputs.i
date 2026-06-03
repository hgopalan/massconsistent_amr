# Case 3: Mt. Hood with Time-Varying Winds and Turbulence
# Tests mass-consistent wind solver with high-elevation alpine terrain
# Terrain: Mt. Hood region, Oregon (high-altitude SRTM terrain)

# Terrain file (user-generated from SRTM data)
terrain_file = terrain.csv

# Reference wind: 10 m/s from southwest at 10 m AGL
U_ref = 10.0
V_ref = -5.0
z_ref = 10.0

# Aerodynamic roughness length [m] (alpine terrain with sparse vegetation)
z0 = 0.05

# Horizontal grid spacing [m]
dx = 50.0
dy = 50.0

# Vertical grid spacing [m] for adequate resolution at high elevation
dz = 30.0

# Domain height [m] above maximum terrain elevation
domain_height = 300.0

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
plot_file = plt_case3_output
