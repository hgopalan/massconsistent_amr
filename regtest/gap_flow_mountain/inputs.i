# Gap Flow Parameterization Test
# Tests: Pressure-driven flow through a mountain gap/pass
# Configuration: Mountain valley gap with enhanced channeling

# Terrain file (21x11 grid representing a mountain gap/valley)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 8.0    # Moderate synoptic wind from west
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 30.0

# Domain height [m] above maximum terrain elevation
domain_height = 500.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Gap Flow Parameterization
enable_gap_flow = true
gap_flow_orientation = 90.0           # Gap oriented north-south (90 degrees)
gap_flow_width = 300.0                # Gap width 300 m (narrow pass)
gap_flow_depth = 400.0                # Gap depth 400 m (elevation range)
gap_flow_pressure_coefficient = 1.0   # Standard pressure coefficient
gap_flow_speedup_max = 3.0            # Maximum 3× speedup (typical for narrow gaps)
gap_flow_center_x = 500.0             # Gap center at x=500 m
gap_flow_center_y = 250.0             # Gap center at y=250 m
gap_flow_transition_width = 200.0     # Transition zone 200 m
gap_flow_vertical_extent = 800.0      # Gap flow extends to 800 m AGL

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 50 m AGL
extract_agl  = 50.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_gap_flow
