# Holtslag-De Bruin Stability Functions Test
# Tests: Alternative stability correction for very stable conditions
# Uses Holtslag-De Bruin (1988) instead of Businger-Dyer formulation

# Terrain file (flat domain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 8.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 200.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable stability correction
enable_stability_correction = true
stability_length = 50.0  # Stable conditions (L > 0)

# Use Holtslag-De Bruin stability functions
use_holtslag_stability = true

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_holtslag_stability
