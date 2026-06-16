# AIJ Case A: Enhanced - Isolated Square Building Wake
# Extended 15H far-wake model with smooth Gaussian lateral spreading
# Building: H=20, W=20, L=20. Centered at x=150, y=100.

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
domain_height = 100.0
alpha_h = 1.0
alpha_v = 1.0

# Solver parameters
mlmg_verbose = 0
max_grid_size = 32
tol_rel = 1.e-8

# Output files
plot_file = plt_aij_case_a_enhanced
extract_agl = 10.0
extract_file = case_aij_a_extract_enhanced.csv

# Enhanced: Extended far-wake and Gaussian profile
enable_oblique_scaling = false
enable_tall_building_correction = false
enable_gaussian_profile = true
enable_upwind_recirculation = false
enable_corner_acceleration = false
enable_horseshoe_vortex = false
enable_extended_farwake = true
enable_variance_correction = false
enable_yoshie_two_layer = false
