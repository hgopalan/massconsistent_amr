# Phase 4 Feature 26: Conditional Stability Model Selection (Ri_b-based)
# Tests: Automatic selection between Businger-Dyer and Holtslag-De Bruin
# Verifies smooth transitions with no discontinuities at Ri_b threshold

# Terrain file (flat domain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 8.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 25.0
dy = 25.0
dz = 20.0

# Domain height [m]
domain_height = 200.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable stability correction
enable_stability_correction = true
stability_length = 75.0  # Very stable conditions (strong inversion)

# Phase 4: Enable adaptive stability model selection based on Ri_b
enable_ri_b_stability_selection = true
ri_b_threshold = 0.1

# MLMG solver settings (quiet)
mlmg_verbose = 0
max_grid_size = 32

# Extract wind at multiple heights to verify profile
extract_agl_multi = 5.0 10.0 20.0 50.0
extract_file = wind_profile.csv

# Output plotfile
plot_file = plt_phase4_ri_b_stability
