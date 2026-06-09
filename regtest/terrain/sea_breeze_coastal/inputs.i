# Sea Breeze Coastal Test
# Tests: Thermal circulation from land-sea temperature contrast
# Configuration: Flat coastal terrain with sea breeze parameterization

# Terrain file (5x5 grid, flat coastal terrain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 5.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

# Grid spacing [m]
dx = 100.0
dy = 100.0
dz = 50.0

# Domain height [m] above maximum terrain elevation
domain_height = 500.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Sea Breeze Parameterization (Thermal Circulation)
enable_thermal_circulation = true
thermal_temperature_contrast = 5.0          # Land warmer by 5 K (daytime sea breeze)
thermal_reference_temperature = 300.0       # Reference temperature [K]
thermal_coefficient = 1.0                   # Scaling coefficient
thermal_vertical_decay_height = 1000.0      # Vertical decay height [m]
thermal_distance_scale = 5000.0             # Horizontal distance scale [m]
thermal_coastline_x = 200.0                 # Coastline at x=200 m
thermal_coastline_y = 200.0                 # Coastline at y=200 m
thermal_coast_normal_x = 1.0                # Coast normal pointing inland (east)
thermal_coast_normal_y = 0.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 50 m AGL
extract_agl  = 50.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_sea_breeze
