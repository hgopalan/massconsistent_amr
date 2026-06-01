# Power-Law Profile Wind Solver Test (Flat Terrain)
# Tests: power-law wind initialization (Phase 1 Feature 1) on flat domain
# Profile: u(z) = U_ref * (z/z_ref)^alpha
# Typical for neutral atmospheric conditions

# Terrain file (3x3 grid, 0-100 m in x and y, all z=0)
terrain_file = terrain.csv

# Power-law initialization mode (Phase 1 Feature 1)
init_mode = powerlaw
U_ref = 8.0
V_ref = 0.0
z_ref = 10.0
powerlaw_exponent = 0.143  # ~1/7 typical for neutral conditions

# Grid spacing [m] -- gives a 2x2x4 grid for fast CI runs
dx = 50.0
dy = 50.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent, default tolerances)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_powerlaw_flat
