# Regression test for diurnal roughness length variations
# Tests time-dependent variation of aerodynamic roughness length z₀(t)

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

# Feature 7: Diurnal Roughness
enable_diurnal_roughness = true
roughness_amplitude      = 0.3
roughness_phase_offset   = 0.0
diurnal_time_of_day      = 14.0

plot_file     = plt_diurnal_roughness
mlmg_verbose  = 0
