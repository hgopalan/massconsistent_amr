# Gaussian Hill Wind Solver Test
# Tests: terrain-following mass-consistent wind on a Gaussian hill
# Terrain: 11x11 grid over a 300x300 m domain, peak elevation 50 m at centre

# Terrain file (pre-generated 11x11 Gaussian hill)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (open terrain / short grass)
z0 = 0.03

# Horizontal grid spacing [m] (matches terrain point spacing)
dx = 30.0
dy = 30.0

# Vertical grid spacing [m] -- coarse spacing keeps CI runtime short
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Output plotfile
plot_file = plt_gaussian_hill

plot_vars = u v w vel_magnitude terrain_z
