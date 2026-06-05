# Spatially-Varying Canopy Wind Solver Test
# Tests: MacDonald et al. (2000) canopy model with spatially-varying forest parameters

# Terrain file (4x4 flat grid, 0-150 m in x and y, all z=0)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 50 m AGL (above canopy)
U_ref = 10.0
V_ref = 0.0
z_ref = 50.0

# Base aerodynamic roughness length [m] for ground surface
z0 = 0.05

# Grid spacing [m] -- moderately fine vertical resolution to resolve canopy
dx = 50.0
dy = 50.0
dz = 5.0

# Domain height [m] above maximum terrain elevation
domain_height = 200.0

# Canopy model parameters (MacDonald et al. 2000)
enable_canopy = true
canopy_file = canopy.csv
canopy_height = 15.0              # Default forest canopy height [m]
frontal_area_index = 0.20         # Default frontal area index
plan_area_index = 0.20            # Crown area / ground area
canopy_drag_coeff = 0.2           # Standard canopy drag coefficient

# Do not use exponential profile for this test (just MacDonald)
use_exponential_profile = false

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent for CI)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 10 m AGL (within canopy) and at 30 m AGL (above canopy)
extract_agl  = 10.0
extract_file = wind_extract_10m.csv

# Output plotfile
plot_file = plt_canopy_spatially_varying
