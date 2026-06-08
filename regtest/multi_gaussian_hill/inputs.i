# Multi-Gaussian Hill Test (Synthetic Terrain Mode)
# Tests mass-consistent wind solver using synthetic terrain parm parse option

terrain_file = synthetic

# Synthetic terrain configuration (EXPERIMENTAL)
synthetic_type = multi_gaussian_hill
synthetic_xmin = 0.0
synthetic_xmax = 300.0
synthetic_ymin = 0.0
synthetic_ymax = 300.0
synthetic_nx = 11
synthetic_ny = 11

synthetic_peaks = 50.0 30.0
synthetic_sigmas = 60.0 40.0
synthetic_centers_x = 100.0 200.0
synthetic_centers_y = 150.0 150.0

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.03

# Horizontal grid spacing [m]
dx = 30.0
dy = 30.0

# Vertical grid spacing [m]
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL and write to CSV
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_multi_gaussian_hill
