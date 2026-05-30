# Building Array Test - Phase 2
# Tests: Wake superposition and street canyon effects for multiple buildings
# Configuration: 3x3 regular array of buildings (30m tall, 20m x 20m)
# Street width: 40m (H/W = 0.75 -> skimming flow regime)

# Terrain file (flat surface at z=0)
terrain_file = terrain.csv

# Buildings from CSV file (3x3 array)
building_file = buildings.csv

# Enable wake model with superposition
enable_wake = true
wake_superposition = true          # Use quadratic superposition for overlapping wakes
wake_c1 = 0.9                       # Cavity length coefficient (Lr = c1 * H)
wake_c2 = 0.3                       # Wake deficit coefficient
wake_separation_length = 3.0        # Wake extends to 3*H downwind

# Enable street canyon effects
enable_street_canyon = true
street_canyon_reduction = 0.3       # Velocity reduction factor in canyon (70% reduction)

# Reference wind: 10 m/s from west (along +x direction) at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m] -- fine resolution for building wakes and street canyons
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

# Output plotfile
plot_file = plt_building_array

# Extract wind field at 10m AGL for visualization
extract_agl = 10.0
extract_file = wind_array_10m.csv
