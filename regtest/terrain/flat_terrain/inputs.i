# Flat Terrain Wind Solver Test
# Tests: mass-consistent solver on a flat (z=0) domain
# Verifies that the solver runs without error and the MLMG Poisson solve
# converges on the simplest possible geometry.

# Terrain file (3x3 grid, 0-100 m in x and y, all z=0)
terrain_file = terrain.csv

# Reference wind: 5 m/s from west at 10 m AGL
U_ref = 5.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m] -- gives a 2x2x2 grid for fast CI runs
dx = 50.0
dy = 50.0
dz = 50.0

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
