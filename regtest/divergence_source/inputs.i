# Divergence Source Terms Test
# Tests: Non-zero RHS in mass-consistency equation (convective plume)
# Applies constant source term to simulate mass injection

# Terrain file (flat domain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 30.0

# Domain height [m] above maximum terrain elevation
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable divergence source terms
# Positive source creates expansion (convective plume)
enable_divergence_source = true
divergence_source_constant = 0.01  # Source term [1/s]

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_divergence_source
