# Wake Model Test: Complex Shapes on Multiple Gaussian Hills
# Verifies oriented rectangular, cylindrical, and pitched-roof building wake implementations
# on a complex multi-hill terrain with quadratic superposition and blending.

# Terrain file (pre-generated multi-Gaussian hill)
terrain_file = terrain.csv

# Buildings from CSV file
building_file = buildings.csv

# Enable wake model and settings
enable_wake = true
wake_model_type = rockle
wake_c1 = 0.9           # Cavity length coefficient
wake_c2 = 0.3           # Wake deficit coefficient
wake_separation_length = 3.0  # Wake extent factor

# Enable rooftop vortex and blending configurations
enable_rooftop_vortex = true
wake_blending_scale_factor = 0.5

# Reference wind: 12 m/s from west (along +x direction) at 10 m AGL
U_ref = 12.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Requested grid spacing [m]
dx = 10.0
dy = 10.0
dz = 5.0

# Domain height [m] above maximum obstacle elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 1
max_grid_size = 32
tol_rel       = 1.e-8

# Output plotfile
plot_file = plt_wake_complex_gaussian_hills

# Extract wind field at 15m AGL (above ground level) for visualization/verification.
# 15m is chosen as a representative height to capture building wakes and rooftop vortices
# while remaining within the active boundary layer above local terrain features.
extract_agl = 15.0
extract_file = wind_wake_15m.csv
