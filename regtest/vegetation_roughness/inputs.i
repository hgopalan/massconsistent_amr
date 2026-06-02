# Vegetation Attenuation Factor for Roughness Test
# Tests: Roughness modification based on vegetation state
# Simulates seasonal variation (e.g., LAI-based adjustment)

# Terrain file (flat domain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.5  # Base roughness (forest canopy)

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 30.0

# Domain height [m] above maximum terrain elevation
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable vegetation roughness factor
enable_vegetation_roughness = true
vegetation_state = 4.0        # LAI value (Leaf Area Index)
vegetation_state_type = 0     # Type 0 = LAI-based

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_vegetation_roughness
