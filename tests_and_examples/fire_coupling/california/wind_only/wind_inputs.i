# Wind solver configuration - California wind_only scenario
# Mass-consistent wind diagnostic with powerlaw profile
# Domain: 10 km x 10 km x 0.3 km
# Grid: 156 x 156 x 38 cells (dx=dy=64m, dz=8m)
# Terrain: SRTM-based California coastal terrain (400-700m elevation)

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
# California: northwesterly wind at 5 m/s (coastal conditions)
U_ref = 5.0
V_ref = 0.0
W_ref = 0.0
z_ref = 10.0

# Powerlaw wind profile: U(z) = U_ref * (z/z_ref)^alpha
# alpha = 0.2 typical for complex terrain
wind_profile = powerlaw
powerlaw_exponent = 0.2

# Surface roughness (z0 in meters)
# California coastal, typical z0 = 0.08m
z0 = 0.08

# Turbulence anisotropy coefficients
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

# Output settings
write_plotfile = 1
plot_interval = 1
plot_fields = velocity,pressure,terrain

# Time settings for steady-state solve
max_time = 600.0
nsteps = 1

# Terrain specification
terrain_file = terrain.csv
