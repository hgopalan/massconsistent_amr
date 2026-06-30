# Wind solver configuration - Colorado one-way coupling scenario
# Domain: 10 km x 10 km x 0.3 km
# Grid: 156 x 156 x 38 cells

xmin = 0.0
ymin = 0.0
zmin = 0.0
dx = 64.0
dy = 64.0
dz = 8.0
domain_height = 300.0

nx = 156
ny = 156
nz = 38

# Reference wind conditions (10m height)
U_ref = 8.0
V_ref = 0.0
W_ref = 0.0
z_ref = 10.0

# Powerlaw wind profile
wind_profile = powerlaw
powerlaw_exponent = 0.2

# Surface roughness
z0 = 0.1

# Turbulence anisotropy
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (to avoid divergence)
mlmg_verbose = 0
mlmg_max_iter = 200
mlmg_max_fmg_iter = 0
mlmg.num_pre_smooth = 8
mlmg.num_post_smooth = 8
mlmg_bottom_solver = bicgstab

# Solver convergence
tol_rel = 1.0e-8

# Boundary conditions
bc_type = dirichlet

# Output settings
write_plotfile = 1
plot_interval = 1
plot_fields = velocity,pressure,terrain

# Time settings for steady-state solve
max_time = 600.0
nsteps = 1

# Terrain specification
terrain_file = ../terrain.csv
