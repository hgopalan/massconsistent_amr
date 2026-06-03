# Regression test for exponential boundary layer wind decay
# Tests wind speed decay above boundary layer depth

terrain_file  = terrain.csv
dx            = 30.0
dy            = 30.0
dz            = 25.0
domain_height = 100.0

init_mode     = loglaw
U_ref         = 10.0
V_ref         = 0.0
z_ref         = 10.0
z0            = 0.03

alpha_h       = 1.0
alpha_v       = 1.0

# Feature 9: Exponential Boundary Layer Decay
enable_bl_decay         = true
bl_depth_param          = 80.0
decay_height_scale      = 20.0
bl_transition_height    = 10.0

plot_file     = plt_bl_decay
mlmg_verbose  = 0
