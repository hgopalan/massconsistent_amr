# Height-Dependent Alpha_v Test
# Tests: height-dependent vertical anisotropy coefficient (Phase 1 Feature 2)
# Alpha_v varies linearly from surface to domain top
# Terrain: Gaussian hill for realistic terrain-following scenario

# Terrain file (pre-generated 11x11 Gaussian hill)
terrain_file = terrain.csv

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

# Phase 1 Feature 2: Height-dependent alpha_v
use_height_dependent_alpha_v = true
alpha_v_surface = 0.5   # Strong vertical adjustment near surface
alpha_v_top = 2.0       # Weaker vertical adjustment aloft

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_alphav_height
