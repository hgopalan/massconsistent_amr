# Valley Channeling Test
# Tests: Valley channeling factor that aligns wind with valley axis
# Verifies that wind direction rotates toward valley axis and speed adjusts
# based on valley geometry

# Terrain file (V-shaped valley running north-south)
terrain_file = terrain.csv

# Enable valley channeling
enable_valley_channeling = true
valley_axis_angle_deg = 90.0          # Valley axis runs north-south (90° from x-axis)
valley_width = 800.0                  # Narrow valley (venturi effect expected)
valley_depth = 250.0                  # Moderately deep valley
valley_channeling_strength_max = 0.8  # Strong channeling
valley_speedup_factor_narrow = 1.3    # Speed-up for narrow valleys
valley_slowdown_factor_wide = 0.85    # Slowdown for wide valleys

# Reference wind: 10 m/s from northwest (diagonal to valley)
# This should be rotated toward north-south alignment
U_ref = 7.07   # 10 * cos(45°)
V_ref = 7.07   # 10 * sin(45°)
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.05

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 500.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32
tol_rel       = 1.e-8

# Output plotfile
plot_file = plt_valley_channeling

# Extract wind field at 10 m AGL
extract_agl = 10.0
extract_file = wind_extract.csv
