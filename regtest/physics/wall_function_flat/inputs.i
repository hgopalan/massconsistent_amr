# ==========================================================================
# Wall function test case - Flat surface with log-law boundary conditions
# ==========================================================================

# Terrain file (flat surface)
terrain_file = terrain_flat.csv

# Wind initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid resolution
dx = 10.0
dy = 10.0
dz = 5.0
domain_height = 100.0

# Wall function parameters
# NEW REQUIREMENT: Enable log-law wall function instead of no-slip
enable_wall_functions = true
enable_terrain_wall_function = true
wall_function_blend_height = 2.0
wall_function_max_distance = 3.0

# Mass-consistent solver
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 1
tol_rel = 1.e-8
mlmg_max_iter = 200

# Output
plot_file = plt_wall_function_flat
max_grid_size = 32

# Extract profiles for validation
extract_agl = 5.0 10.0 25.0 50.0 100.0
extract_file = wall_function_profile.csv
