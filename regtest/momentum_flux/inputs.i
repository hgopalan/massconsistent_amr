# Regression test for momentum flux output fields
# Tests computation of τ_x, τ_y, and u* output fields

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

# Feature 8: Momentum Flux Output (enabled by default)
# This feature is always computed when using standard wind_solver

plot_file     = plt_momentum_flux
extract_agl   = 15.0
extract_file  = wind_extract.csv
mlmg_verbose  = 0
