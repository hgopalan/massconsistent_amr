# Polygon Wake Test: Complex Shapes with Huber-Snyder Model
# Tests: Huber-Snyder wake parameterization for L-shaped, T-shaped, and U-shaped buildings
# Verifies that polygon buildings compute wake deficits with Huber-Snyder wake zone transitions

# Terrain file (flat surface at z=0)
terrain_file = terrain.csv

# Buildings from CSV file - includes L, T, U-shaped polygon buildings
building_file = buildings.csv

# Enable wake model with Huber-Snyder parameterization
enable_wake = true
wake_model = huber_snyder
wake_c1 = 1.5           # Cavity length coefficient (longer than Röckle)
wake_c2 = 0.4           # Wake deficit coefficient
wake_separation_length = 4.0  # Farther wake extent for Huber-Snyder
C_h = 0.5               # Huber-Snyder horizontal turbulence parameter
C_v = 0.3               # Huber-Snyder vertical turbulence parameter

# Reference wind: 10 m/s from west (along +x direction) at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m] -- reasonable resolution for polygon building wakes
dx = 5.0
dy = 5.0
dz = 5.0

# Domain height [m] above maximum obstacle elevation
domain_height = 300

# Domain extents
x_min = 0.0
x_max = 300.0
y_min = 0.0
y_max = 200.0

# Output location for verification
output_file = wind_wake_10m.csv
output_height = 10.0
