# Deaves-Harris Wind Profile Test
terrain_file = terrain.csv

# Deaves-Harris initialization
init_mode = deaves_harris
U_ref = 15.0            # High wind: 15 m/s
V_ref = 0.0
z_ref = 10.0
z0 = 0.1
bl_depth_param = 1000.0  # Gradient height zg [m]

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0
domain_height = 200.0

alpha_h = 1.0
alpha_v = 1.0

mlmg_verbose  = 0
plot_file = plt_deaves_harris
