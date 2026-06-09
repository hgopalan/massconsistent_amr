# Multi-Height Extraction Test
# Tests: extracting wind fields at multiple heights (10m, 50m, 100m AGL)
# This demonstrates the new multi-height extraction feature

# Terrain file (same as gaussian_hill test)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.03

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 150.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Multi-height extraction (NEW FEATURE)
# Extract wind at 10m, 50m, and 100m above ground level
# This will create three files: wind_extract_10m.csv, wind_extract_50m.csv, wind_extract_100m.csv
# Note: Use space-separated values (AMReX standard)
extract_agl  = 10.0 50.0 100.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_multiheight
