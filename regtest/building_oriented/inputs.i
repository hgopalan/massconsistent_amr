# Solver Enhancement Regression Test: Building Orientation Effects
#
# This test validates arbitrary building orientation support in the wake model.
# The same building geometry is tested with multiple orientations to verify that
# the wind-relative effective dimensions are correctly computed.
#
# Test setup:
# - Rectangular building with non-aligned orientation (45 degrees)
# - Flat terrain
# - Log-law wind initialization from northwest (U_ref=10m/s, V_ref=5m/s)
# - Validates that effective building width and length adapt to wind direction

# Domain and grid
nx = 40
ny = 40
nz = 15
x_lo = -100.0
x_hi = 1100.0
y_lo = -100.0
y_hi = 1100.0
z_lo = 0.0
z_hi = 300.0

# Grid spacing
dx = 30.0
dy = 30.0
dz = 20.0

# Wind initialization (log-law)
# Diagonal wind direction to test orientation effects
init_mode = loglaw
U_ref = 8.66
V_ref = 5.0
z_ref = 10.0
z0 = 0.1

# Terrain (flat)
terrain_file = terrain.csv

# Building with 45-degree rotation
building_file = buildings.csv
enable_wake = true
wake_c1 = 0.9
wake_c2 = 0.3
wake_separation_length = 3.0

# Solver options
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 1
tol_rel = 1.0e-8
max_grid_size = 32

# Output
plot_file = plt_oriented_building
extract_agl = 10.0
extract_file = oriented_building_profile.csv
