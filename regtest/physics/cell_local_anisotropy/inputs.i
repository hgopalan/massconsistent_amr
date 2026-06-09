# Spatially-Varying Variational Anisotropy Regression Test
# Tests: fully 3D spatially-varying anisotropic weighting tensor A(x,y,z)
# based on local terrain slope, local Richardson number, and local Froude number

# Terrain file (11x11 Gaussian hill)
terrain_file = terrain.csv

# Reference wind
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 25.0
domain_height = 100.0

# Base Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable Cell-Local Spatially-Varying Variational Anisotropy
enable_cell_local_anisotropy = true
anisotropy_source = all
anisotropy_slope_scale = 0.25
anisotropy_decay_height = 100.0
anisotropy_ri_gamma = 1.0
anisotropy_ri_beta = 0.5
anisotropy_fr_min = 0.1
temperature_file = "temperature.csv"
temperature_gradient = 0.005 # Stable potential temperature gradient

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind
extract_agl  = 15.0
extract_file = wind_extract.csv
plot_file = plt_cell_local_anisotropy
