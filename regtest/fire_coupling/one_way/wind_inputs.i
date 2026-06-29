# Wind solver inputs for one-way coupling test
# Simple flat terrain for fire coupling tests

# Domain configuration
amr.max_level = 0
amr.n_cell = 32 32 16
dx = 31.25
dy = 31.25
dz = 30.0
domain_height = 300.0

# Physical domain bounds (meters)
geometry.prob_lo = 0.0 0.0 0.0
geometry.prob_hi = 1000.0 1000.0 400.0

# Terrain configuration
terrain.type = flat
terrain.zs0 = 100.0

# Reference wind
wind.U_ref = 10.0
wind.V_ref = 0.0
wind.z_ref = 10.0

# Roughness
wind.z0 = 0.1

# Solver settings
wind.alpha_h = 1.0
wind.alpha_v = 1.0
wind.mlmg_verbose = 0
wind.tol_rel = 1.e-8
wind.max_iter = 200

# Output
wind.plot_interval = 0
wind.write_fine_plotfile = 0
