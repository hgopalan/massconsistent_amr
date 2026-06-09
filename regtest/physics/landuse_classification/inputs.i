# Core Feature: Land-use Roughness Classification Feature Test
# Tests: NLCD-compatible land-use category mapping to aerodynamic roughness (z0)
# This test verifies that land-use classification correctly assigns z0 values
# based on categorical mapping and that they are used in initialization

# Terrain file - Gaussian hill
terrain_file = terrain.csv

# Land-use classification file with NLCD categories
landuse_file = landuse.csv

# Log-law initialization using land-use derived roughness
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Default roughness (may be overridden by land-use classification)
z0 = 0.1

# Grid parameters
dx = 30.0
dy = 30.0
dz = 25.0
domain_height = 100.0

# Mass-consistent parameters
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver (silent mode)
mlmg_verbose  = 0
max_grid_size = 32

# Core Feature land-use classification parameters
# Enable land-use based roughness assignment
enable_landuse_classification = true

# Interpolation method for land-use derived z0 (e.g., IDW)
landuse_interp_method = idw

# Extract wind at 15 m AGL to verify land-use effects on profile
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_landuse_classification
