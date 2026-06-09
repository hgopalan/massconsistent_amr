# Phase 4 Feature 25: Wake Deficit Superposition Refinement
# Tests: Distance-weighted blending of overlapping wake zones
# Verifies smooth velocity field transitions at wake boundaries

# Flat terrain
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 20.0
dy = 20.0
dz = 15.0

# Domain height [m]
domain_height = 150.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Building configuration - two overlapping buildings
# to test wake blending at intersection
enable_buildings = true
n_buildings = 2

# Building 1: upstream
bldg_xmin[0] = 100.0
bldg_xmax[0] = 120.0
bldg_ymin[0] = 80.0
bldg_ymax[0] = 100.0
bldg_zmin[0] = 0.0
bldg_zmax[0] = 20.0

# Building 2: downstream (wake zone overlaps)
bldg_xmin[1] = 140.0
bldg_xmax[1] = 160.0
bldg_ymin[1] = 95.0
bldg_ymax[1] = 115.0
bldg_zmin[1] = 0.0
bldg_zmax[1] = 25.0

# Wake model configuration
enable_wakes = true
wake_model_type = 0  # Röckle model
wake_c1 = 0.9
wake_c2 = 0.3
wake_separation_length = 3.0

# Phase 4: Enable adaptive wake superposition
enable_adaptive_wakes = true

# MLMG solver settings (quiet)
mlmg_verbose = 0
max_grid_size = 32

# Extract wind at 10 m AGL for verification
extract_agl = 10.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_phase4_wake_blending
