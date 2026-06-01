# Wall Function Regression Test: Adaptive Activation
# Tests automatic activation/deactivation based on grid resolution
# Wall functions should only activate where dz/z0 is in appropriate range

# Domain size
nx = 32
ny = 32
nz = 32
dx = 10.0
dy = 10.0
dz = 2.0         # Fine vertical spacing for testing

# Flat terrain
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1         # Surface roughness: dz/z0 = 2.0/0.1 = 20 (within range)

# Wall functions with adaptive activation
enable_wall_functions = true
enable_terrain_wall_function = true
wall_function_blend_height = 2.0
wall_function_max_distance = 3.0

# Adaptive activation: automatically enable/disable based on grid resolution
wall_function_enable_adaptive = true
wall_function_adaptive_threshold = 30.0    # Max dz/z0 ratio
wall_function_adaptive_min_cells = 3.0     # Min cells in log layer

# Expected behavior:
# - dz = 2.0 m, z0 = 0.1 m → dz/z0 = 20 → wall function ACTIVE
# - If z0 were 0.05 m → dz/z0 = 40 → wall function INACTIVE (too coarse)
# - If z0 were 0.5 m → dz/z0 = 4 → wall function ACTIVE (borderline)

# Mass-consistent solver
alpha_h = 1.0
alpha_v = 1.0
solve_mass_constraint = true

# Output
plot_file = plt_wall_function_adaptive
max_grid_size = 32

# Extract profiles for validation
extract_agl = 2.0 5.0 10.0 25.0 50.0
extract_file = wall_function_adaptive_profile.csv
