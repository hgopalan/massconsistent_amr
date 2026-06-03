# Phase 4 Feature 27: Orographic Model Activation Thresholds
# Tests: Jackson-Hunt model activated only if Fr > 0.1 AND slope > 5%
# Verifies model is active on steep terrain with sufficient wind

# Gaussian hill terrain for testing orographic effects
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0  # Sufficient wind for Fr > 0.1
V_ref = 0.0
z_ref = 10.0
z0 = 0.15

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 30.0

# Domain height [m]
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Stability (neutral)
enable_stability_correction = false

# Enable orographic speedup model
enable_orographic_speedup = true
hill_length_scale = 200.0
speedup_factor_max = 2.0
separation_factor = 0.3
smoothing_factor = 0.5

# Phase 4: Enable adaptive activation with Froude and slope thresholds
enable_froude_slope_thresholds = true
froude_threshold = 0.1
slope_threshold = 0.05  # 5% minimum slope

# Brunt-Väisälä frequency for Froude number [1/s]
brunt_vaisala_freq = 0.01

# MLMG solver settings (quiet)
mlmg_verbose = 0
max_grid_size = 32

# Extract wind at 20 m AGL for verification
extract_agl = 20.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_phase4_orographic_thresholds
