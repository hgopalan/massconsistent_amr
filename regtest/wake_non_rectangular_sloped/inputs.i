# Wake Model Test: Non-Rectangular and Sloped Roof Buildings
# Verifies cylinder and pitched roof building wake implementations

# Terrain file (flat surface at z=0)
terrain_file = terrain.csv

# Buildings from CSV file
building_file = buildings.csv

# Enable wake model
enable_wake = true
wake_c1 = 0.9           # Cavity length coefficient
wake_c2 = 0.3           # Wake deficit coefficient
wake_separation_length = 3.0  # Wake extent factor

# Reference wind: 12 m/s from west (along +x direction) at 10 m AGL
U_ref = 12.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m]
dx = 5.0
dy = 5.0
dz = 5.0

# Domain height [m] above maximum obstacle elevation
domain_height = 150.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 1
max_grid_size = 32
tol_rel       = 1.e-8

# Output plotfile
plot_file = plt_wake_non_rectangular_sloped

# Extract wind field at 10m AGL for visualization
extract_agl = 10.0
extract_file = wind_wake_10m.csv
