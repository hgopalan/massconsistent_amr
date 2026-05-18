# Synthetic RAWS Wind Solver Test
# Tests: terrain-following mass-consistent wind using RAWS interpolation mode
# Terrain: 11x11 grid over a 300x300 m domain, peak elevation 50 m at centre

# Terrain file
terrain_file = terrain.csv

# Initialization mode: "raws" uses velocity file with IDW interpolation
init_mode = raws
velocity_file = velocity.csv

# Horizontal grid spacing [m]
dx = 30.0
dy = 30.0

# Vertical grid spacing [m] -- coarse spacing keeps CI runtime short
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL and write to CSV
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_raws_synthetic
