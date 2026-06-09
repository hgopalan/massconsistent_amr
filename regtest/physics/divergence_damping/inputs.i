# Divergence Damping Filter Test
# Tests: post-solve divergence reduction via Laplacian smoothing of lambda
# Verifies: max |∇·u| reduces by 30-50% after damping

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

# Feature 11: Divergence Damping
enable_divergence_damping = true
damping_coefficient = -1.0        # Auto-compute: 0.05 * min(dx,dy,dz)^2
damping_iterations = 2

mlmg_verbose = 0
max_grid_size = 32

extract_agl = 15.0
extract_file = wind_extract.csv

plot_file = plt_divergence_damping
