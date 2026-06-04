# IEC 61400-1:2019 Von Kármán Spectrum Test
# Tests: IEC61400 turbulence intensity model with Von Kármán spectral synthesis
# Validates that the solver accepts IEC61400 as intensity_model and properly
# generates fluctuations using the Von Kármán spectrum over Gaussian hill terrain

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
plot_file = plt_iec61400_vk

# ============================================================================
# IEC 61400-1 Synthetic Turbulence Configuration (Von Kármán)
# ============================================================================

# Master enable flag for synthetic turbulence generation
enable_synthetic_turbulence = true

# Turbulence model parameters
# ================================

# Spectral model: VonKarman or Kaimal
turbulence_spectrum_model = VonKarman

# IEC 61400-1 turbulence intensity model
# NEW: This now accepts "IEC61400" (previously only PowerLaw, Logarithmic, Constant)
turbulence_intensity_model = IEC61400

# Reference hub height for IEC61400 [m]
iec_hub_height = 90.0

# IEC turbulence category: A (0.16), B (0.14), or C (0.12)
# Determines reference turbulence intensity at hub height
iec_category = B

# Coherence (spatial correlation) decay model: Gaussian or Exponential
# NEW: Also accepts QuadraticExponential and PowerLaw
turbulence_coherence_model = Gaussian

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

# Spectral synthesis parameters
# ============================

# Number of frequency bins for spectral discretization (32, 64, 128, 256)
# Higher values give better spectral resolution but slower computation
turbulence_n_freq_bins = 64

# Random field generation
# ==================================

# Random seed for reproducible field generation
# Use any unsigned integer; same seed produces identical fields
turbulence_random_seed = 42

# Time-series generation and export
# ==============================

# Export format: 'bts' (TurbSim binary format) compatible with NREL OpenFAST
turbulence_export_format = bts

# Output filename for synthetic turbulence fluctuations
turbulence_output_file = turbulence_iec61400_vk.bts
