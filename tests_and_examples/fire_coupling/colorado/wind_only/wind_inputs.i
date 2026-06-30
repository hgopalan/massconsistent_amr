# Wind solver configuration - Colorado wind_only scenario
# Mass-consistent wind diagnostic with powerlaw profile
# Domain: 10 km x 10 km x 0.3 km
# Grid resolution: 256m horizontal, 30m vertical (coarse for fast single-processor testing)
# Terrain: SRTM-based Colorado mountains (2100-2400m elevation)

# Grid spacing and domain height
dx = 256.0
dy = 256.0
dz = 30.0
domain_height = 300.0

# Reference wind conditions (10m height)
# Colorado: westerly wind at 8 m/s
U_ref = 8.0
V_ref = 0.0
z_ref = 10.0

# Wind profile type: powerlaw, loglaw, uniform, raws, ekman_spiral, sounding, surface_data
# Powerlaw wind profile: U(z) = U_ref * (z/z_ref)^alpha
# alpha = 0.2 typical for complex terrain
init_mode = powerlaw
powerlaw_exponent = 0.2

# Surface roughness (z0 in meters)
# Colorado mountain terrain, typical z0 = 0.1m
z0 = 0.1

# Turbulence anisotropy coefficients
alpha_h = 1.0    # Horizontal anisotropy
alpha_v = 1.0    # Vertical anisotropy

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
