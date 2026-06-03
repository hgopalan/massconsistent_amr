# Regression test for ageostrophic wind balance boundary conditions
# Tests geostrophic wind computation with Coriolis parameters

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

# Feature 10: Ageostrophic Wind Balance
enable_ageostrophic_balance         = true
ageostrophic_latitude               = 45.0
ageostrophic_pressure_grad_x        = 0.0
ageostrophic_pressure_grad_y        = -1.0
ageostrophic_air_density            = 1.225
ageostrophic_fraction               = 0.15

plot_file     = plt_ageostrophic_balance
mlmg_verbose  = 0
