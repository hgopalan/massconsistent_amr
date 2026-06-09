# O'Brien Vertical Velocity Adjustment Procedure Test
# Tests: redistribute vertical divergence residuals to force w = 0 at the domain top.

terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 20.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# O'Brien Vertical Velocity Adjustment
enable_obrien_w_adjustment = true

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32

extract_agl = 90.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_obrien
