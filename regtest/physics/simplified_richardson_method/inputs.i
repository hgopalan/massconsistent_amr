# Regression test for Simplified Richardson Number Method
# Tests fast Ri_b to stability class mapping and Obukhov length computation

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

# Feature: Simplified Richardson Number Method
enable_simplified_richardson = true

# Non-neutral stability for Richardson number computation
enable_stability_correction = true
stability_length = 100.0

plot_file     = plt_simplified_richardson
mlmg_verbose  = 0
