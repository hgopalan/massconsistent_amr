# Pasquill-Gifford Stability Classification Test (Feature 10)
# Tests: Classic atmospheric stability classes (A-F) based on wind speed and solar radiation
# Domain: Simple flat terrain
# Condition: Daytime, moderate insolation, light wind → Class B (moderately unstable)

# Terrain file (3x3 grid, flat terrain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 3.0            # Light wind: 3 m/s
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Feature 10: Pasquill-Gifford stability classification
# Input: wind_speed = 3 m/s, solar_radiation = 500 W/m² (moderate insolation)
# Expected output: Class B (moderately unstable)
# Mapped to: L ≈ -100 m (unstable conditions)
enable_pg_stability = true
solar_radiation = 500.0      # Moderate insolation [W/m²]
is_nighttime = false         # Daytime conditions

# The code will:
# 1. Classify stability using PG lookup table
# 2. Map class B to L ≈ -100 m
# 3. Apply stability correction to wind profile

enable_stability_correction = true  # Use the computed L from PG class

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
plot_file = plt_pasquill_gifford
