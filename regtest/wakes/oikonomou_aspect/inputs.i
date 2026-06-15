# Oikonomou Aspect-Ratio Dependent Cavity Correction Test
# Tests: Aspect-ratio dependent cavity zone modeling for elongated buildings
# Verifies that cavity zone length and deficit scale with building L/W ratio

# Terrain file (flat surface at z=0)
terrain_file = terrain.csv

# Buildings from CSV file (includes both square and elongated buildings)
building_file = buildings.csv

# Enable wake model
enable_wake = true
wake_c1 = 0.9           # Cavity length coefficient (Lr = c1 * H)
wake_c2 = 0.3           # Wake deficit coefficient
wake_separation_length = 3.0  # Wake extends to 3*H downwind

# Oikonomou aspect-ratio correction parameters
enable_oikonomou_aspect = true
oikonomou_beta_aspect = 0.25    # Aspect-ratio correction coefficient

# Other enhancements enabled for realistic test
enable_extended_farwake = true
enable_oblique_scaling = true
enable_tall_building_correction = true
enable_upwind_recirculation = true
enable_horseshoe_vortex = true
enable_corner_acceleration = true

# Reference wind: 10 m/s from west (along +x direction) at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m] -- good resolution for cavity zone
dx = 5.0
dy = 5.0
dz = 5.0

# Domain height [m] above maximum obstacle elevation
domain_height = 150.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 1
max_grid_size = 32
tol_rel       = 1.e-8
