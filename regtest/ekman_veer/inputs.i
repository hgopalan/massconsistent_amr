# Ekman Spiral Wind Veer Test
# Tests: Ekman spiral correction for wind direction rotation with height
# Verifies that wind direction veers (rotates) with height due to Coriolis effects
# Expected: Wind should rotate clockwise with height in Northern Hemisphere

# Terrain file (flat 3x3 grid for simple testing)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west (270°) at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (short grass)
z0 = 0.03

# Grid spacing [m] -- fine vertical resolution to capture veer profile
dx = 50.0
dy = 50.0
dz = 10.0

# Domain height [m] above maximum terrain elevation
# High enough to see full veer profile development
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Ekman spiral wind veer parameters
enable_ekman_veer = true
latitude = 45.0              # Mid-latitude Northern Hemisphere
ekman_veer_total = 25.0      # 25 degrees total veer (typical for mid-latitudes)
ekman_veer_height = 150.0    # Most veer in lowest 150 m

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at multiple heights to see veer profile
extract_agl  = 10.0 50.0 100.0 200.0
extract_file = ekman_veer_extract.csv

# Output plotfile
plot_file = plt_ekman_veer
