# Wall Function Regression Test: Unstable Atmospheric Conditions
# Tests stability corrections in wall functions with unstable boundary layer
# L < 0 (unstable conditions, typically daytime with surface heating)

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
U_ref = 12.0        # 12 m/s reference wind
V_ref = 0.0
z_ref = 10.0        # Reference height
z0 = 0.1            # Surface roughness (grass/crops)

# Wall functions with stability corrections
enable_wall_functions = true
enable_terrain_wall_function = true
wall_function_blend_height = 2.0
wall_function_max_distance = 3.0

# Stability correction: Unstable boundary layer
wall_function_enable_stability = true
wall_function_stability_length = -150.0   # L = -150 m (moderately unstable, daytime)

# Mass-consistent solver
alpha_h = 1.0
alpha_v = 1.0
solve_mass_constraint = true

# Output
plot_file = plt_wall_function_unstable
max_grid_size = 32

# Extract profiles at multiple heights for validation
extract_agl = 5.0 10.0 25.0 50.0 100.0
extract_file = wall_function_unstable_profile.csv
