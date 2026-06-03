# Regression test for Roughness Blocking from Buildings
# Tests building-induced roughness contribution to aerodynamic roughness length

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

# Feature: Roughness Blocking from Buildings
enable_roughness_blocking    = true
building_roughness_factor    = 0.04

# Include urban buildings to test roughness interaction
buildings_csv             = buildings.csv
enable_buildings          = true

plot_file     = plt_roughness_blocking
mlmg_verbose  = 0
