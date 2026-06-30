# Wind solver configuration - Colorado two-way coupling scenario
# Two-way coupling: Wind ↔ Fire (fire heating affects wind)
# Domain: 10 km x 10 km x 0.3 km

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
mlmg.agglomeration = 1
mlmg.consolidation = 1
mlmg.nu0 = 2
mlmg.nu1 = 2
mlmg.nu2 = 2
mlmg.verbose = 0
mlmg.num_pre_smooth = 8
mlmg.num_post_smooth = 8
mlmg.bot_smoother = "visc_abs_sing"

# Solver convergence
tol_rel = 1.0e-8
tol_abs = 1.0e-10
max_iter = 200

# Boundary conditions
bc_type = dirichlet

# Heat source (for two-way coupling)
use_heat_source = 1

# Output settings
write_plotfile = 1
plot_interval = 1
plot_fields = velocity,pressure,terrain,heat_source

# Time settings for steady-state solve
max_time = 600.0
nsteps = 1

# Terrain specification
terrain_file = terrain.csv
