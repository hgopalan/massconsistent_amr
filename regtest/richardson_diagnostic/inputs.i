# Regression test for Richardson number boundary layer depth diagnostic
# Tests computation of Richardson number and diagnosed BL depth

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

# Feature 23: Richardson Number BL Depth Diagnostic
enable_bl_depth_diagnostic  = true
richardson_critical        = 0.25
richardson_min_wind_shear  = 0.001

plot_file     = plt_richardson_diagnostic
mlmg_verbose  = 0
