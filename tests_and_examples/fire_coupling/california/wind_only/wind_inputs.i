# Wind solver configuration - California wind_only scenario
# Mass-consistent wind diagnostic with powerlaw profile
# Domain: 10 km x 10 km x 0.3 km
# Grid resolution: 64m horizontal, 8m vertical
# Terrain: SRTM-based California coastal terrain (400-700m elevation)

# Grid spacing and domain height (coarsened 2x in x,y and 2x in z for faster computation)
dx = 128.0
dy = 128.0
dz = 16.0
domain_height = 300.0

# Reference wind conditions (10m height)
# California: northwesterly wind at 5 m/s (coastal conditions)
U_ref = 5.0
V_ref = 0.0
z_ref = 10.0

# Wind profile type: powerlaw, loglaw, uniform, raws, ekman_spiral, sounding, surface_data
# Powerlaw wind profile: U(z) = U_ref * (z/z_ref)^alpha
# alpha = 0.2 typical for complex terrain
init_mode = powerlaw
powerlaw_exponent = 0.2

# Surface roughness (z0 in meters)
# California coastal, typical z0 = 0.08m
z0 = 0.08

# Turbulence anisotropy coefficients
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

# Output settings - plot_fields specifies which fields to include in output
# Supported fields: u,v,w,vel_magnitude,u0,v0,w0,lambda,div0,div,terrain_z
# Composite aliases: velocity,pressure,initial_velocity,divergence
plot_fields = velocity,pressure,terrain

# Terrain specification
terrain_file = ../terrain.csv
