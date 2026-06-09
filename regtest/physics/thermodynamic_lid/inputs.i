# Atmospheric Inversion Thermodynamic Lid Test
terrain_file = terrain.csv
time_series_file = time_series.csv
enable_time_varying = true

init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Thermodynamic Lid Feature
enable_thermodynamic_lid = true
thermodynamic_lid_model = carson
thermodynamic_lid_flux_file = flux.csv
thermodynamic_lid_gamma = 0.005
thermodynamic_lid_initial_zi = 100.0
thermodynamic_lid_entrainment_ratio = 0.2
thermodynamic_lid_rho = 1.2
thermodynamic_lid_cp = 1005.0

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0
domain_height = 200.0

alpha_h = 1.0
alpha_v = 1.0

mlmg_verbose  = 0
plot_file = plt_thermo_lid
