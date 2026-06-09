# Wake Model Test: Huber-Snyder Model
# Tests: Huber-Snyder (EPA) wake parameterization for a single rectangular building
# Verifies that wake model correctly computes cavity and far-wake zones
# Compares against Röckle model behavior

# Terrain file (flat surface at z=0)
terrain_file = terrain.csv

# Enable wake model with Huber-Snyder formulation
enable_wake = true
wake_model_type = huber_snyder       # Use Huber-Snyder instead of Röckle
wake_c2 = 0.3                         # Wake deficit coefficient
wake_separation_length = 5.0          # Huber-Snyder uses longer wake (5H)

# Reference wind: 10 m/s from west (along +x direction) at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (typical urban terrain)
z0 = 0.1

# Horizontal grid spacing [m]
dx = 10.0
dy = 10.0

# Vertical grid spacing [m]
dz = 10.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Building geometry (same as wake_single_building for comparison)
# 40m x 20m x 30m building centered at (100, 100)
building_file = buildings.csv

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL (mid-building height)
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_wake_huber_snyder
