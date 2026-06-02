# Orographic Speed-up Test (Jackson & Hunt 1975)
# Tests: Wind speedup over a Gaussian hill with orographic model enabled
# Terrain: 11x11 grid over a 300x300 m domain, peak elevation 50 m at centre

# Terrain file (pre-generated 11x11 Gaussian hill)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
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

# Enable orographic speedup (Jackson & Hunt 1975 model)
enable_orographic_speedup = true
orographic_hill_length_scale = 60.0       # Match Gaussian hill sigma
orographic_speedup_factor_max = 1.8       # Moderate speedup on crest
orographic_separation_factor = 0.3        # Lee-side separation
orographic_smoothing_factor = 0.6         # Smooth transitions

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL and write to CSV
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_orographic_speedup
