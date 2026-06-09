# Drag Coefficient Diagnostic Test
# Tests: Surface drag coefficient C_d and heat flux diagnostics 
# Tests with spatially-varying roughness to verify drag coefficient computation

# Terrain file (3x3 grid, flat terrain)
terrain_file = terrain.csv

# Position-dependent roughness file
z0_file = roughness.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1  # Default roughness (overridden by z0_file)

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 10 m AGL to verify drag coefficient at standard height
extract_agl  = 10.0
extract_file = wind_extract.csv

# Output plotfile (will contain drag_coeff and heat_flux variables)
plot_file = plt_drag_diagnostic
