# Regression test for Heat Flux Diagnostics Enhancement
# Tests extended surface heat flux computations (SHF, LHF, diagnostics)

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

# Feature: Heat Flux Diagnostic Enhancement
enable_heat_flux_diagnostics = true
heat_flux_theta_star         = 0.1

# Non-neutral stability for realistic heat flux interaction
enable_stability_correction  = true
stability_length             = 100.0

plot_file     = plt_heat_flux_diagnostics
mlmg_verbose  = 0
