# Regression test for Power-Law Wind Profile Above Boundary Layer
# Tests power-law wind profile option as alternative to exponential decay

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

# Feature: Power-Law Wind Profile Above Boundary Layer
enable_bl_decay              = true
bl_depth_param               = 80.0
decay_height_scale           = 20.0
bl_transition_height         = 10.0
enable_power_law_profile     = true
power_law_exponent           = 0.15

plot_file     = plt_power_law_profile
mlmg_verbose  = 0
