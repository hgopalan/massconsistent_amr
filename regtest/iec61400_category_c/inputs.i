# IEC 61400-1:2019 Category C (Low Turbulence) Test
# Tests: IEC61400 Category C (12% at hub) turbulence intensity model
# Validates that solver correctly handles low-turbulence sites with
# QuadraticExponential coherence model

# Terrain file
terrain_file = terrain.csv

# Reference wind conditions at hub height
U_ref = 12.0
V_ref = 0.0
z_ref = 90.0

# Aerodynamic roughness length [m]
z0 = 0.03

# Horizontal grid spacing [m]
dx = 30.0
dy = 30.0

# Vertical grid spacing [m]
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 150.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 90 m AGL (hub height) and write to CSV
extract_agl  = 90.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_iec61400_cat_c

# ============================================================================
# IEC 61400-1 Category C: Low Turbulence Sites (12% at hub)
# ============================================================================

# Master enable flag for synthetic turbulence generation
enable_synthetic_turbulence = true

# Turbulence model parameters
# ================================

# Spectral model: VonKarman or Kaimal
turbulence_spectrum_model = VonKarman

# IEC 61400-1 turbulence intensity model
turbulence_intensity_model = IEC61400

# Reference hub height for IEC61400 [m]
iec_hub_height = 90.0

# IEC turbulence category: A (0.16), B (0.14), or C (0.12)
# Category C is for low-turbulence, wind-farm friendly sites
iec_category = C

# Coherence (spatial correlation) decay model
# Testing QuadraticExponential (NEW in C++ parser)
turbulence_coherence_model = QuadraticExponential

# Integral length scales [m] - spatial correlation distances
turbulence_length_scale_u = 300.0
turbulence_length_scale_v = 200.0
turbulence_length_scale_w = 120.0

# Coherence decay factors [1/m]
turbulence_coherence_decay_vertical = 0.008
turbulence_coherence_decay_lateral = 0.006

# Anisotropy ratios - velocity component RMS ratios
turbulence_anisotropy_ratio_v = 0.80
turbulence_anisotropy_ratio_w = 0.50

# Spectral synthesis parameters
# ============================

# Number of frequency bins for spectral discretization
turbulence_n_freq_bins = 96

# Random field generation
# ==================================

# Random seed for reproducible field generation
turbulence_random_seed = 99

# Time-series generation and export
# ==============================

# Export format: 'bts' (TurbSim binary format) compatible with NREL OpenFAST
turbulence_export_format = bts

# Output filename for synthetic turbulence fluctuations
turbulence_output_file = turbulence_iec61400_cat_c.bts
