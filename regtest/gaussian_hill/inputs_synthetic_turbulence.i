# Gaussian Hill with Synthetic Turbulence Regression Test
# Tests the full synthetic turbulence pipeline (Phase 1-3) over complex terrain
# Validates parameter parsing and BTS export for OpenFAST compatibility

# ============================================================================
# Wind Solver Configuration
# ============================================================================

# Terrain file (Gaussian hill 11x11 grid over 300x300 m domain)
terrain_file = terrain.csv

# Reference wind: 10 m/s from west at 10 m AGL
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (open terrain / short grass)
z0 = 0.03

# Grid spacing [m]
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

# Terrain-aligned extraction
extract_agl  = 15.0
extract_file = wind_extract_synthetic.csv

# Output plotfile
plot_file = plt_gaussian_hill_turb

# ============================================================================
# Synthetic Turbulence Configuration (Phase 1-3)
# ============================================================================

# Master enable flag for synthetic turbulence generation
enable_synthetic_turbulence = true

# Phase 1: Turbulence Parameters
# ================================

# Spectral model selection
# Options: VonKarman (default, isotropic), Kaimal (empirical, IEC standard)
turbulence_spectrum_model = VonKarman

# Turbulence intensity profile model
# Options: PowerLaw (default, I(z)=I_ref*(z/z_ref)^alpha)
#          Logarithmic (I(z)=I_0*ln(z/z0)/ln(z_ref/z0))
#          Constant (I(z)=constant)
turbulence_intensity_model = PowerLaw

# Coherence (cross-component correlation) decay model
# Options: Gaussian (default, exp(-k*distance^2))
#          Exponential (exp(-k*distance))
turbulence_coherence_model = Gaussian

# Turbulence intensity reference value [fraction]
# Typical range: 0.10 - 0.20 (10% - 20%)
# Comment: Represents TI at z_intensity_ref height
turbulence_intensity_ref = 0.12

# Reference height for turbulence intensity [m AGL]
# Typical: 10 m (standard meteorological height)
turbulence_z_intensity_ref = 10.0

# Power-law exponent for intensity variation with height
# Typical range: 0.10 - 0.20
# Comment: Used when turbulence_intensity_model = PowerLaw
#          I(z) = I_ref * (z/z_ref)^alpha
turbulence_intensity_exponent = 0.14

# Integral length scales (spatial correlation length)
# Represents the distance scale over which fluctuations are correlated

# Longitudinal (u-component) integral length scale [m]
# Typical: 100 - 500 m (height and stability dependent)
turbulence_length_scale_u = 300.0

# Lateral (v-component) integral length scale [m]
# Typical ratio to u: 0.6 - 0.8
turbulence_length_scale_v = 200.0

# Vertical (w-component) integral length scale [m]
# Typical ratio to u: 0.3 - 0.5
turbulence_length_scale_w = 120.0

# Coherence decay factors [1/m]
# Quantify how spatial coherence decays with vertical or lateral separation

# Vertical coherence decay factor [1/m]
# Typical range: 0.006 - 0.010
# Higher values = faster coherence decay with height
turbulence_coherence_decay_vertical = 0.008

# Lateral coherence decay factor [1/m]
# Typical range: 0.004 - 0.008
turbulence_coherence_decay_lateral = 0.006

# Anisotropy ratios (velocity component RMS ratios)
# Represents the typical ratios of lateral and vertical turbulence to longitudinal

# Ratio of v-velocity RMS to u-velocity RMS
# Typical: 0.75 - 0.85
turbulence_anisotropy_ratio_v = 0.80

# Ratio of w-velocity RMS to u-velocity RMS
# Typical: 0.45 - 0.55
turbulence_anisotropy_ratio_w = 0.50

# Phase 2: Random Field Generation
# ==================================

# Random seed for reproducible field generation
# Any unsigned integer value enables deterministic output
# Default: 12345
turbulence_random_seed = 12345

# Phase 3: Time-Series & Export
# ==============================

# Export format specification
# Currently supported: bts (TurbSim binary format)
turbulence_export_format = bts

# Output filename for turbulence fluctuations
# Will be written in BTS format (binary) with optional .meta ASCII metadata
turbulence_output_file = turbulence_gaussian_hill.bts
