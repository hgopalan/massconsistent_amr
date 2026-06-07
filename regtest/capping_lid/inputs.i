# Atmospheric Inversion Capping Lid Test
terrain_file = terrain.csv

init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Capping Lid Feature
enable_capping_lid = true
capping_lid_height = 100.0

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0
domain_height = 200.0

alpha_h = 1.0
alpha_v = 1.0

mlmg_verbose  = 0
plot_file = plt_capping_lid
