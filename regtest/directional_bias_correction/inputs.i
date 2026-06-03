# Phase 1: Directional Bias Correction Feature Test
# Tests: Wind direction and speed bias correction for NWP model systematic errors
# This test verifies that directional and speed biases are correctly applied
# to adjust initial wind fields based on systematic model errors

# Terrain file - Gaussian hill
terrain_file = terrain.csv

# Log-law initialization (will be adjusted by bias correction)
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Roughness
z0 = 0.03

# Grid parameters
dx = 30.0
dy = 30.0
dz = 25.0
domain_height = 100.0

# Mass-consistent parameters
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver (silent mode)
mlmg_verbose  = 0
max_grid_size = 32

# Phase 1 directional bias correction parameters
# Enable directional bias correction for systematic NWP model errors
enable_directional_bias_correction = true

# Constant directional bias (applied uniformly)
# Example: model wind always 30 degrees too far counterclockwise
direction_bias_constant = 30.0  # [degrees]

# Constant speed bias factor (multiplicative)
# Example: model overestimates wind speed by 5%
speed_bias_factor = 1.05

# Optional: periodic (sinusoidal) bias that varies with wind direction
# This captures more complex directional-dependent biases
enable_periodic_bias = false
direction_bias_amplitude = 15.0  # [degrees] amplitude of periodic bias
direction_bias_phase = 0.0       # [degrees] phase offset

# Extract wind at 15 m AGL to verify bias correction
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_directional_bias_correction
