# Flat Terrain Wind Solver Test with Turbines
terrain_file = terrain.csv

# Enable Turbine Wake Modeling
enable_turbine_wake = true
turbine_file = turbines.csv
turbine_wake_model_type = jensen
turbine_wake_superposition = quadratic

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m]
dx = 10.0
dy = 10.0
dz = 10.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent, default tolerances)
mlmg_verbose  = 0
max_grid_size = 32

# Output plotfile
plot_file = plt_flat_terrain
