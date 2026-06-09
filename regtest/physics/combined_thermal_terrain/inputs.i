# Combined Thermal Stratification and Kinematic BC Test
# Tests: Both buoyancy effects and kinematic terrain BC together
# Configuration: Gaussian hill with stable stratification

# Terrain file (Gaussian hill)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
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

# Thermal stratification with buoyancy
enable_buoyancy_stratification = true
temperature_file = temperature.csv
temperature_reference = 300.0
buoyancy_coefficient = 1.0
buoyancy_timescale = 10.0

# Kinematic terrain-following boundary condition
enable_terrain_kinematic_bc = true
terrain_bc_relaxation = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_combined
