# Feature 3: Land Use-Based Power-Law Exponents Test
# Tests: Spatially-varying power-law exponent based on land use type
# Different terrain types have different wind shear profiles

# Terrain file (flat domain)
terrain_file = terrain.csv

# Power-law initialization
init_mode = powerlaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
powerlaw_exponent = 0.143  # Default exponent (will be overridden by land use)

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 30.0

# Domain height [m] above maximum terrain elevation
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Feature 3: Enable land use-based power-law exponents
landuse_file = landuse.csv

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_landuse_powerlaw
