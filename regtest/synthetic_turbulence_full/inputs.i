# Synthetic Turbulence Full Pipeline Regression Test (Phase 1-3)
# Tests the complete synthetic turbulence framework: parameter parsing,
# random field synthesis (Phase 2), time-series generation (Phase 3),
# and BTS export for OpenFAST compatibility

# ============================================================================
# Wind Solver Configuration
# ============================================================================

# Terrain file (use simple Gaussian hill for testing)
terrain_file = terrain.csv

# Reference wind conditions
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.03

# Grid spacing [m] (coarse for faster test execution)
dx = 30.0
dy = 30.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings
mlmg_verbose  = 0
max_grid_size = 32

# Terrain-aligned extraction for wind field verification
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_turbulence_full

# ============================================================================
# Synthetic Turbulence Configuration (Phase 1-3)
# ============================================================================

# Master enable flag for synthetic turbulence generation (Phase 1-3)
enable_synthetic_turbulence = true

# Phase 1: Turbulence Parameters
# ================================

# Spectral model: VonKarman or Kaimal
turbulence_spectrum_model = VonKarman

# Turbulence intensity profile: PowerLaw, Logarithmic, or Constant
turbulence_intensity_model = PowerLaw

# Coherence (spatial correlation) decay model: Gaussian or Exponential
turbulence_coherence_model = Gaussian

# Turbulence intensity reference value [fraction]
# Represents TI at z_intensity_ref height (typically 10m)
turbulence_intensity_ref = 0.14

# Reference height for turbulence intensity [m AGL]
turbulence_z_intensity_ref = 10.0

# Power-law exponent for intensity variation with height
# I(z) = I_ref * (z/z_ref)^alpha
turbulence_intensity_exponent = 0.14

# Integral length scales [m] - spatial correlation distances
# u-component (longitudinal, wind direction)
turbulence_length_scale_u = 300.0

# v-component (lateral, horizontal perpendicular to wind)
turbulence_length_scale_v = 200.0

# w-component (vertical)
turbulence_length_scale_w = 120.0

# Coherence decay factors [1/m] - how quickly spatial correlations decay
# Vertical coherence decay factor
turbulence_coherence_decay_vertical = 0.008

# Lateral coherence decay factor
turbulence_coherence_decay_lateral = 0.006

# Anisotropy ratios - velocity component RMS ratios
# v-velocity RMS to u-velocity RMS ratio (typical: 0.75-0.85)
turbulence_anisotropy_ratio_v = 0.80

# w-velocity RMS to u-velocity RMS ratio (typical: 0.45-0.55)
turbulence_anisotropy_ratio_w = 0.50

# Phase 2: Random Field Generation
# ==================================

# Random seed for reproducible field generation
# Use any unsigned integer; same seed produces identical fields
turbulence_random_seed = 42

# Phase 3: Time-Series & Export
# ==============================

# Export format: currently only 'bts' (TurbSim binary format) is supported
turbulence_export_format = bts

# Output filename for synthetic turbulence fluctuations
# Will write binary .bts file (compatible with NREL OpenFAST)
# Also generates .meta ASCII metadata file with same basename
turbulence_output_file = turbulence_synthetic.bts
