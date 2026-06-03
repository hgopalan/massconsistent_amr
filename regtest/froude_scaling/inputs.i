# Regression test for Froude number height scaling with terrain blocking
# Tests height-dependent terrain blocking intensity variation

terrain_file  = terrain.csv
dx            = 30.0
dy            = 30.0
dz            = 25.0
domain_height = 100.0

init_mode     = log_law
U_ref         = 10.0
V_ref         = 0.0
z_ref         = 10.0
z0            = 0.03

alpha_h       = 1.0
alpha_v       = 1.0

# Feature 21: Froude Number Height Scaling
enable_terrain_blocking                    = true
enable_froude_height_scaling               = true
terrain_blocking_brunt_vaisala_frequency   = 0.01

plot_file     = plt_froude_scaling
mlmg_verbose  = 0
