# Synthetic Turbulence Generators Regression Test
# Exercises the new spectrum models: GP_LLJ, NWTC, USWTPP, HIT

# ============================================================================
# Wind Solver Configuration
# ============================================================================

terrain_file = terrain.csv
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03
dx = 30.0
dy = 30.0
dz = 25.0
domain_height = 100.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose  = 0
max_grid_size = 32
extract_agl  = 15.0
extract_file = wind_extract.csv
plot_file = plt_turbulence_generators

# ============================================================================
# Synthetic Turbulence Configuration
# ============================================================================

enable_synthetic_turbulence = true

# Spectral model: GP_LLJ, NWTC, USWTPP, or HIT
turbulence_spectrum_model = GP_LLJ

# Model specific parameters
gp_llj_jet_height = 80.0
nwtc_scaling_parameter = 1.2
uswtpp_weight = 0.5

# Turbulence intensity profile: PowerLaw, Logarithmic, or Constant
turbulence_intensity_model = PowerLaw
turbulence_intensity_ref = 0.14
turbulence_z_intensity_ref = 10.0
turbulence_intensity_exponent = 0.14

# Coherence decay model: Gaussian or Exponential
turbulence_coherence_model = Gaussian
turbulence_coherence_decay_vertical = 0.008
turbulence_coherence_decay_lateral = 0.006

# Integral length scales [m]
turbulence_length_scale_u = 300.0
turbulence_length_scale_v = 200.0
turbulence_length_scale_w = 120.0

# Anisotropy ratios
turbulence_anisotropy_ratio_v = 0.80
turbulence_anisotropy_ratio_w = 0.50

# Random seed
turbulence_random_seed = 42

# Time-series generation and export
turbulence_export_format = bts
turbulence_output_file = turbulence_synthetic.bts
