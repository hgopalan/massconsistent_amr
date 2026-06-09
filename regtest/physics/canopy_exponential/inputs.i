# Canopy Shaw-Pereira Exponential Profile Test
# Tests: Shaw & Pereira (1982) exponential decay within canopy
# This test validates the combined MacDonald displacement height
# with exponential wind speed decay within the canopy.

# Terrain file (4x4 flat grid, 0-150 m in x and y, all z=0)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 40 m AGL (above canopy)
U_ref = 10.0
V_ref = 0.0
z_ref = 40.0

# Base aerodynamic roughness length [m]
z0 = 0.05

# Grid spacing [m] -- fine vertical resolution to resolve within-canopy profile
dx = 50.0
dy = 50.0
dz = 2.5

# Domain height [m] above maximum terrain elevation
domain_height = 150.0

# Canopy model parameters
enable_canopy = true
canopy_height = 15.0              # Canopy height [m]
frontal_area_index = 0.30         # Dense canopy
plan_area_index = 0.25
canopy_drag_coeff = 0.25

# Enable Shaw-Pereira exponential decay within canopy
use_exponential_profile = true
canopy_attenuation = 2.5          # Attenuation coefficient (α)

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32

# Extract at 7.5 m AGL (middle of canopy)
extract_agl  = 7.5
extract_file = wind_extract_7.5m.csv

# Output plotfile
plot_file = plt_canopy_exponential
