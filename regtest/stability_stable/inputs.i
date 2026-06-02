# Stability Correction Test (Stable Conditions)
# Tests: Non-neutral log-law with Businger-Dyer stability corrections 
# Stable stratification: L > 0 (nocturnal boundary layer)

# Terrain file (3x3 grid, flat terrain for clarity)
terrain_file = terrain.csv

# Log-law initialization with stability correction
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Enable stability correction for stable conditions
enable_stability_correction = true
stability_length = 100.0  # Stable: L > 0 (suppresses vertical mixing)

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_stability_stable
