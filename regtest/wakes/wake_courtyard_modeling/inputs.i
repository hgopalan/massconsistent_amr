# Courtyard Modeling Test: Internal Void Zones and Complex Building Layouts
# Tests: Polygon buildings with internal courtyards modeled as void zones
# Verifies that void zones are properly excluded from wake calculations
# and that superposition accounts for complex building arrangements

# Terrain file (flat surface at z=0)
terrain_file = terrain.csv

# Buildings from CSV file - includes polygon with internal void zone
building_file = buildings.csv

# Enable wake model with Röckle parameterization
enable_wake = true
wake_model = rockle
wake_c1 = 0.9           # Cavity length coefficient (Lr = c1 * H)
wake_c2 = 0.3           # Wake deficit coefficient
wake_separation_length = 3.0  # Wake extends to 3*H downwind

# Enable superposition to handle multiple building interactions
enable_superposition = true

# Reference wind: 10 m/s from west (along +x direction) at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m]
dx = 5.0
dy = 5.0
dz = 5.0

# Domain height [m] above maximum obstacle elevation
domain_height = 300

# Domain extents
x_min = 0.0
x_max = 300.0
y_min = 0.0
y_max = 300.0

# Output location for verification
output_file = wind_wake_10m.csv
output_height = 10.0
