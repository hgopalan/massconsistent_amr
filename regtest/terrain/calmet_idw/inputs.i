# CALMET-style IDW interpolation features Test
# Tests: idw_rmax1, idw_rmax2, idw_r1, idw_r2

# Terrain file
terrain_file = terrain.csv

# Initialization mode: "raws" uses velocity file
init_mode = raws
velocity_file = velocity.csv

# Reference/background parameters for Step 1 blending
U_ref = 6.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

# CALMET IDW parameters
idw_rmax1 = 150.0   # Surface layer max radius of influence (far station at 295,295 ignored near 0,0)
idw_rmax2 = 500.0   # Upper layers max radius of influence (both stations active)
idw_r1 = 100.0      # Surface blending radius
idw_r2 = -1.0       # No blending for upper layers

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 20.0

# Domain height [m]
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at multiple heights to verify blending & rmax
extract_agl  = 10.0 50.0
extract_file = calmet_idw_extract.csv

# Output plotfile
plot_file = plt_calmet_idw
