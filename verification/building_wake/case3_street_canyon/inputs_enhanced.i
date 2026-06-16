# Case 3: Enhanced - 2D Building Array with Upwind Recirculation Zone
# Includes upwind stagnation/recirculation zone modeling
# Two buildings with 30m canyon between them

terrain_file = terrain.csv
building_file = buildings.csv
enable_wake = true
wake_c1 = 0.9
wake_c2 = 0.3
wake_separation_length = 3.0

# Reference wind
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Domain and grid
dx = 5.0
dy = 5.0
dz = 5.0
domain_height = 150.0
alpha_h = 1.0
alpha_v = 1.0

# Solver parameters
mlmg_verbose = 0
max_grid_size = 32
tol_rel = 1.e-8

# Output files
plot_file = plt_case3_enhanced
extract_agl = 5.0
extract_file = case3_extract_enhanced.csv

# Enhanced: upwind recirculation zone enabled
enable_oblique_scaling = false
enable_tall_building_correction = false
enable_gaussian_profile = false
enable_upwind_recirculation = true
enable_corner_acceleration = false
enable_horseshoe_vortex = false
enable_extended_farwake = false
enable_variance_correction = false
enable_yoshie_two_layer = false
