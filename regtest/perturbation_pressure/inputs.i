# Perturbation Pressure Gradient Test
# Tests: pressure-Poisson solver for enhanced velocity correction
# Verifies: pressure residual converges to tolerance, velocity field improves
# NOTE: This feature is OPTIONAL and disabled by default

terrain_file = terrain.csv

U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

dx = 30.0
dy = 30.0
dz = 25.0

domain_height = 100.0

alpha_h = 1.0
alpha_v = 1.0

# Feature 15: Perturbation Pressure Gradient (OPT-IN)
# Default is FALSE; must be explicitly enabled
enable_perturbation_pressure = true
pressure_tol_rel = 1.0e-6
pressure_max_iter = 100
pressure_scale = 0.5

mlmg_verbose = 0
max_grid_size = 32

extract_agl = 15.0
extract_file = wind_extract.csv

plot_file = plt_perturbation_pressure
