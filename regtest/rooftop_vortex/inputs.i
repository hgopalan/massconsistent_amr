# Solver Enhancement Regression Test: Rooftop Vortex Verification
#
# This test validates the rooftop vortex parameterization in the cavity zone
# of a single building. The vertical velocity component should show characteristic
# upward-downward-upward circulation pattern due to the vortex.
#
# Test setup:
# - Single rectangular building (30m x 20m x 30m tall)
# - Flat terrain for simplicity
# - Log-law initialization with 10 m/s reference wind
# - Fine vertical resolution to capture vortex structure

# Domain and grid
nx = 40
ny = 40
nz = 20
x_lo = -100.0
x_hi = 1100.0
y_lo = -100.0
y_hi = 1100.0
z_lo = 0.0
z_hi = 400.0

# Grid spacing
dx = 30.0
dy = 30.0
dz = 20.0

# Wind initialization (log-law)
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Terrain (flat)
terrain_file = terrain.csv

# Building (CSV file with optional rotation)
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
plot_file = plt_rooftop_vortex

# Optional: Extract vertical profile through cavity zone for analysis
extract_agl = 5.0
extract_file = rooftop_vortex_profile.csv
