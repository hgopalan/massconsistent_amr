# Wake Model Test: EPA AERMOD PRIME
# Tests: AERMOD PRIME wake parameterization for a single rectangular building
# Verifies that wake model correctly computes cavity and far-wake zones
# using the Projected Building Area (PBA) method

# Terrain file (flat surface at z=0)
terrain_file = terrain.csv

# Buildings from CSV file
building_file = buildings.csv

# Enable wake model with AERMOD PRIME formulation
enable_wake = true
wake_model_type = aermod_prime       # Use AERMOD PRIME (EPA regulatory model)
wake_c2 = 0.3                         # Wake deficit coefficient
wake_separation_length = 10.0         # AERMOD PRIME uses longer wake (10H)

# Reference wind: 10 m/s from west (along +x direction) at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (typical urban terrain)
z0 = 0.1

# Grid spacing [m] -- reasonable resolution for building wake
dx = 5.0
dy = 5.0
dz = 5.0

# Domain height [m] above maximum obstacle elevation
domain_height = 150.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32
tol_rel       = 1.e-8

# Output plotfile
plot_file = plt_wake_aermod_prime

# Extract wind field at 15 m AGL (mid-building height)
extract_agl = 15.0
extract_file = wind_extract.csv
