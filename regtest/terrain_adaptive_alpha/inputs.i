# Terrain-Adaptive Alpha Coefficients Test (Feature 12)
# Tests: Spatially-varying α_h and α_v based on terrain slope and curvature
# Domain: Gaussian hill with varying slopes
# Expected behavior:
#   - Flat regions (low slope): α_v ≈ 1.0 (isotropic adjustment)
#   - Steep slopes: α_v << 1.0 (preserve terrain-following)
#   - Ridge tops: α_v reduced (constrain vertical motion)
#   - Valleys: α_v increased (allow vertical adjustment)

# Terrain file (Gaussian hill, 5x5 grid for slope variation)
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

# Feature 12: Enable terrain-adaptive alpha coefficients
# Compute α_v based on local terrain slope and curvature
enable_terrain_adaptive_alpha = true
alpha_h_base = 1.0          # Base horizontal coefficient
alpha_v_flat = 1.0          # Vertical coefficient for flat terrain
alpha_slope_scale = 0.25    # Slope decay parameter (typical: 0.2-0.3)

# Expected results:
# - Hill crest (high curvature, moderate slope): α_v ≈ 0.4-0.6
# - Hill sides (steep slope): α_v ≈ 0.2-0.4
# - Valley/flat base (low slope): α_v ≈ 0.9-1.0

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_terrain_alpha
