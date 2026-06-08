# Inputs for Randomized Hill with 20 Turbines and Time-Varying Wind
terrain_file = terrain.csv

# Enable Turbine Wake Modeling
enable_turbine_wake = true
turbine_file = turbines.csv
turbine_wake_model_type = jensen
turbine_wake_superposition = quadratic

# Log-law Initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 90.0
z0 = 0.1

# Enable time-varying boundary conditions
enable_time_varying = true
time_series_file = time_series.csv

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 15.0

# Domain height [m] above maximum terrain elevation
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Output plotfile
plot_file = plt_randomized_hill
