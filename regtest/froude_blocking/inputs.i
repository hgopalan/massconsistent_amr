# Froude Number Terrain Blocking Test
# Tests: Flow blocking around steep terrain when Fr < 1
# Configuration: Gaussian hill with stable stratification (low Froude number)

# Terrain file (11x11 grid over Gaussian hill)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 5.0    # Low wind speed for Fr < 1
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 200.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Froude Number Terrain Blocking
enable_terrain_blocking = true
terrain_blocking_brunt_vaisala_frequency = 0.02     # Strong stratification [1/s]
terrain_blocking_reduction_factor = 0.6             # Strong blocking
terrain_blocking_transition_froude = 1.0            # Transition at Fr=1
terrain_blocking_flank_enhancement = 1.3            # Channeling on flanks
terrain_blocking_reference_temperature = 288.0      # Reference temperature [K]
terrain_blocking_lapse_rate = 0.01                  # Stable lapse rate [K/m]

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_froude_blocking
