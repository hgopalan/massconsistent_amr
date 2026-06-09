# Flatirons Terrain Wind Solver Test with Turbines
# Tests: terrain-following wind with wind turbine wake models
# Terrain: 21x21 grid over a 1000x1000 m domain, sloping down from West to East with foothills

# Terrain file (sloping terrain)
terrain_file = terrain.csv

# Enable Turbine Wake Modeling
enable_turbine_wake = true
turbine_file = turbines.csv
turbine_wake_model_type = jensen
turbine_wake_superposition = quadratic

# Reference wind: 12 m/s from West (270 degrees) at 10 m AGL
U_ref = 12.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (mixed grass and forest)
z0 = 0.1

# Horizontal grid spacing [m]
dx = 50.0
dy = 50.0

# Vertical grid spacing [m]
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 250.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 80 m AGL (hub height) and write to CSV
extract_agl  = 80.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_flatirons_turbines
