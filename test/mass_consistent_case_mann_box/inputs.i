# Case: Mann Box Spectral Tensor on Gaussian Hill
# Tests Mann Box anisotropic turbulence model with complex terrain
# Terrain: 21x21 grid over 500x500 m domain, peak elevation 75 m
# Mann Box captures terrain-induced anisotropy better than isotropic models

# Terrain file (pre-generated 21x21 Gaussian hill)
terrain_file = terrain.csv

# Reference wind: 12 m/s from west at 20 m AGL
U_ref = 12.0
V_ref = 0.0
z_ref = 20.0

# Aerodynamic roughness length [m] (grassland)
z0 = 0.05

# Horizontal grid spacing [m]
dx = 25.0
dy = 25.0

# Vertical grid spacing [m]
dz = 20.0

# Domain height [m] above maximum terrain
domain_height = 250.0

# Lagrange anisotropy coefficients (default)
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose = 0
max_grid_size = 32

# Extract wind at 50 m AGL (turbine hub-height representative)
extract_agl = 50.0
extract_file = wind_extract_mann_box.csv

# Output plotfile
plot_file = plt_mann_box_output

# ============================================================================
# Mann Box Turbulence Parameters (Phase 2)
# ============================================================================

# Enable synthetic turbulence
turbulence.enabled = 1

# Use Mann Box spectral tensor model
turbulence.spectrum_model = MannBox

# Intensity model (can use any Phase 1 model with Mann Box)
turbulence.intensity_model = PowerLaw
turbulence.intensity_ref = 0.12
turbulence.z_intensity_ref = 20.0
turbulence.intensity_exponent = 0.14

# Coherence model (any Phase 1 model compatible)
turbulence.coherence_model = Exponential
turbulence.coherence_decay_vertical = 0.008
turbulence.coherence_decay_lateral = 0.006

# ---- Mann Box Specific Parameters ----

# Integral length scales [m]
# These characterize the size of turbulent eddies
turbulence.mann_length_scale_u = 300.0
turbulence.mann_length_scale_v = 210.0
turbulence.mann_length_scale_w = 120.0

# Variance ratios (control anisotropy distribution)
# u: 1.0 (reference), v: ~0.7-0.8, w: ~0.4-0.5
turbulence.mann_variance_u = 1.0
turbulence.mann_variance_v = 0.80
turbulence.mann_variance_w = 0.50

# Asymmetry parameter (controls tensor shape)
# Range: 0.5-2.0, typical 1.0
turbulence.mann_asymmetry_parameter = 1.0

# Eddy lifetime [s] (for time-varying applications)
turbulence.mann_eddy_lifetime = 0.1

# Terrain adaptation factor (multiplicative)
# 1.0 = standard Mann Box, > 1.0 enhances terrain effects
turbulence.mann_terrain_adaptation_factor = 1.0

# Random seed for reproducibility
turbulence.random_seed = 42
