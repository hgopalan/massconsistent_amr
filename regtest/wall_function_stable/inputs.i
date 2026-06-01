# Wall Function Regression Test: Stable Atmospheric Conditions
# Tests stability corrections in wall functions with stable boundary layer
# L > 0 (stable conditions, typically nighttime with surface cooling)

# Domain size
nx = 32
ny = 32
nz = 32
dx = 10.0
dy = 10.0
dz = 5.0

# Flat terrain
terrain_file = terrain.csv

# Log-law initialization with stability correction
init_mode = loglaw
U_ref = 8.0         # 8 m/s reference wind
V_ref = 0.0
z_ref = 10.0        # Reference height
z0 = 0.1            # Surface roughness (grass/crops)

# Wall functions with stability corrections
enable_wall_functions = true
enable_terrain_wall_function = true
wall_function_blend_height = 2.0
wall_function_max_distance = 3.0

# Stability correction: Stable boundary layer
wall_function_enable_stability = true
wall_function_stability_length = 100.0    # L = 100 m (moderately stable, nighttime)

# Mass-consistent solver
alpha_h = 1.0
alpha_v = 1.0
solve_mass_constraint = true

# Output
plot_file = plt_wall_function_stable
max_grid_size = 32

# Extract profiles at multiple heights for validation
extract_agl = 5.0 10.0 25.0 50.0 100.0
extract_file = wall_function_stable_profile.csv
