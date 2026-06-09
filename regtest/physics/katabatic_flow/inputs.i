# Katabatic Flow Test
# Tests: Thermally-driven down-slope flow on inclined terrain
# Terrain: 11x11 grid over a 300x300 m domain, Gaussian hill

# Terrain file (reuse Gaussian hill from orographic test)
terrain_file = terrain.csv

# Reference wind: light 2 m/s from west at 10 m AGL
U_ref = 2.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (open terrain / short grass)
z0 = 0.03

# Horizontal grid spacing [m] (matches terrain point spacing)
dx = 30.0
dy = 30.0

# Vertical grid spacing [m]
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable katabatic slope flows (nighttime cold air drainage)
enable_slope_flows = true
slope_flow_temperature_diff = -8.0              # Surface 8K cooler than air (strong katabatic)
slope_flow_reference_temperature = 300.0        # Reference temperature [K]
slope_flow_empirical_coefficient = 3.0          # Moderate strength [m/s]
slope_flow_vertical_decay_height = 50.0         # Shallow layer [m]
slope_flow_min_slope = 0.05                     # Minimum slope threshold

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 10 m AGL and write to CSV
extract_agl  = 10.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_katabatic_flow
