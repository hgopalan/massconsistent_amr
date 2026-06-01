# Power-Law Profile Wind Solver Test (Complex Terrain)
# Tests: power-law wind initialization (Phase 1 Feature 1) on Gaussian hill
# Terrain: 11x11 grid over a 300x300 m domain, peak elevation 50 m at centre

# Terrain file (pre-generated 11x11 Gaussian hill)
terrain_file = terrain.csv

# Power-law initialization mode (Phase 1 Feature 1)
init_mode = powerlaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
powerlaw_exponent = 0.143  # ~1/7 typical for neutral conditions

# Horizontal grid spacing [m] (matches terrain point spacing)
dx = 30.0
dy = 30.0

# Vertical grid spacing [m] -- coarse spacing keeps CI runtime short
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL and write to CSV
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_powerlaw_gaussian
