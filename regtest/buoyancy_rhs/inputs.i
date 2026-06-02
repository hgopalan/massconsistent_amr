# Buoyancy RHS Method Test
# Tests: Thermal stratification with buoyancy source term added to RHS
# Configuration: Flat terrain with stable temperature stratification

# Terrain file (3x3 grid, flat terrain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 200.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Thermal stratification with buoyancy (RHS method)
enable_buoyancy_stratification = true
temperature_file = temperature.csv
temperature_reference = 300.0       # Reference temperature [K]
buoyancy_coefficient = 1.0          # Tuning parameter
buoyancy_method = "rhs"             # Add buoyancy to RHS instead of velocity

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 50 m AGL
extract_agl  = 50.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_buoyancy_rhs
