# Regression test for Coriolis Latitude Scaling
# Tests latitude-dependent Coriolis parameter computation

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

# Feature: Coriolis Latitude Scaling
enable_coriolis_latitude  = true
domain_latitude           = 45.0

# Enable ageostrophic balance to utilize Coriolis parameter
enable_ageostrophic_balance = true
ageostrophic_latitude       = 45.0

plot_file     = plt_coriolis_latitude
mlmg_verbose  = 0
