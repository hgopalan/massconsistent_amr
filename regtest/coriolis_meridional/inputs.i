# Coriolis Meridional Beta-Effect Test
# Tests: Latitude-dependent Coriolis parameter and Ekman spiral veer scaling.

terrain_file = terrain.csv

U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

z0 = 0.03

# Spacings
dx = 50.0
dy = 500.0
dz = 10.0

domain_height = 200.0

# Enable Coriolis Latitude scaling
enable_coriolis_latitude = true
domain_latitude = 45.0      # Domain center latitude [degrees]

# Enable Ekman veer
enable_ekman_veer = true
ekman_veer_total = 25.0
ekman_veer_height = 100.0

alpha_h = 1.0
alpha_v = 1.0

mlmg_verbose  = 0
max_grid_size = 32

plot_file = plt_coriolis_meridional
