# Polygon Wake Test: Complex Shapes with Röckle Model
# Tests: Röckle wake parameterization for L-shaped, T-shaped, and U-shaped buildings
# Verifies that polygon buildings compute wake deficits with correct orientation and extent

# Terrain file (flat surface at z=0)
terrain_file = terrain.csv

# Buildings from CSV file - includes L, T, U-shaped polygon buildings
building_file = buildings.csv

# Enable wake model with Röckle parameterization
enable_wake = true
wake_model = rockle
wake_c1 = 0.9           # Cavity length coefficient (Lr = c1 * H)
wake_c2 = 0.3           # Wake deficit coefficient
wake_separation_length = 3.0  # Wake extends to 3*H downwind

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
