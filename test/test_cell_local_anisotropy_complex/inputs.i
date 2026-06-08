# Custom inputs.i for complex case
terrain_file = terrain.csv
init_mode = loglaw
U_ref = 5.0
V_ref = 0.0
z_ref = 50.0
z0 = 0.05

dx = 200.0
dy = 200.0
dz = 50.0
domain_height = 1200.0

alpha_h = 1.0
alpha_v = 1.0

enable_cell_local_anisotropy = true
anisotropy_source = all
anisotropy_slope_scale = 0.25
anisotropy_decay_height = 800.0
anisotropy_ri_gamma = 1.2
anisotropy_ri_beta = 0.6
anisotropy_fr_min = 0.1
temperature_file = "temperature.csv"
temperature_gradient = 0.004

mlmg_verbose = 0
max_grid_size = 64
