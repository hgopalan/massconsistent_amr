# Terrain-Aware Temperature Test
# Tests: Temperature profile respects terrain elevation, internal temperature for subsurface cells
# Configuration: Complex terrain with temperature stratification

# Terrain file (3x3 grid with central peak)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Terrain-aware temperature initialization (NEW FEATURE)
# When enabled: cells below ground level (z_agl <= 0) use temperature_interior
# Cells above ground use the temperature profile from temperature_file
enable_terrain_aware_temperature = true
temperature_interior = 283.15           # Internal/subsurface temperature [K] (10°C)
enable_buoyancy_stratification = true
temperature_file = temperature.csv
temperature_reference = 300.0
buoyancy_coefficient = 1.0
buoyancy_timescale = 10.0

# MLMG solver settings (silent)
mlmg_verbose = 0
max_grid_size = 32

# Extract wind at 50 m AGL
extract_agl = 50.0
extract_file = wind_extract.csv

# Output plotfile (includes temperature field)
plot_file = plt_terrain_aware_temp
