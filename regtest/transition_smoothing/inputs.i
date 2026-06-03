# Surface-Layer-to-Mixed-Layer Transition Smoothing Test
# Tests: smooth blending of log-law and mixed-layer profiles
# Verifies: wind shear is smooth (continuous first derivative)
# Expected: no discontinuities in du/dz at transition height

terrain_file = terrain.csv

U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

dx = 30.0
dy = 30.0
dz = 25.0

domain_height = 100.0

alpha_h = 1.0
alpha_v = 1.0

# Feature 24: Surface-Layer-to-Mixed-Layer Transition Smoothing
enable_transition_smoothing = true
transition_height_scale = 150.0
bl_transition_height = 300.0

mlmg_verbose = 0
max_grid_size = 32

extract_agl = 15.0
extract_file = wind_extract.csv

plot_file = plt_transition_smoothing
