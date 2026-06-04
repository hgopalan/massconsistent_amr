# Gaussian Hill with Mann Box Turbulence Model
# 
# This test case demonstrates the Mann Box anisotropic spectral tensor model
# applied to the Gaussian Hill benchmark terrain.
#
# Key Features:
# - Terrain: Gaussian hill (11x11 grid, 300x300 m domain, 50 m peak)
# - Turbulence: Mann Box spectral tensor model
# - Spectrum: Diagonal (S_uu, S_vv, S_ww) + off-diagonal (S_uv, S_uw, S_vw)
# - Length scales: L_u = 300 m, L_v = 210 m, L_w = 120 m (anisotropic)
# - Preset: Neutral atmospheric stability
#
# References:
#   Mann, J. (1994). J. Fluid Mech., 273, 141-168
#   Mann, J. (1998). Prob. Eng. Mech., 13(4), 269-282

################################################################################
# BASIC CONFIGURATION
################################################################################

# Terrain file (pre-generated 11x11 Gaussian hill)
terrain_file = terrain.csv

# Reference wind conditions
# Wind speed 12 m/s from west (U > 0 means from positive X direction)
U_ref = 12.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m] (open terrain / short grass)
z0 = 0.03

################################################################################
# GRID CONFIGURATION
################################################################################

# Horizontal grid spacing [m] (matches terrain point spacing 30 m)
dx = 30.0
dy = 30.0

# Vertical grid spacing [m]
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

################################################################################
# SOLVER CONFIGURATION
################################################################################

# MLMG (Multi-Level Multi-Grid) solver parameters
mlmg_verbose     = 0        # Verbosity level (0=silent, 1=verbose)
max_grid_size    = 32       # Max grid points per patch
mg_maxiter       = 100      # Max multigrid iterations
mg_rtol          = 1.0e-11  # Relative tolerance for convergence
mg_atol          = 1.0e-14  # Absolute tolerance for convergence

################################################################################
# SYNTHETIC TURBULENCE - MANN BOX MODEL
################################################################################

# Enable synthetic turbulence generation
enable_synthetic_turbulence = 1

# Turbulence spectrum model: VonKarman, Kaimal, or MannBox
# Note: Mann Box is the default for complex terrain
turbulence_spectrum_model = MannBox

# Turbulence intensity profile model
turbulence_intensity_model = PowerLaw

# Reference turbulence intensity at 10 m height
turbulence_intensity_ref = 0.12

# Reference height for turbulence intensity [m]
turbulence_z_intensity_ref = 10.0

# Power law exponent for turbulence intensity height scaling
turbulence_intensity_exponent = 0.14

################################################################################
# MANN BOX SPECTRAL TENSOR PARAMETERS
################################################################################

# Integral length scales [m]
# Typical values for neutral boundary layer:
#   L_u = 300-400 m (u-component)
#   L_v ≈ 0.7*L_u   (v-component, reduced anisotropy)
#   L_w ≈ 0.4*L_u   (w-component, reduced anisotropy)
turbulence_length_scale_u = 300.0
turbulence_length_scale_v = 210.0
turbulence_length_scale_w = 120.0

# Velocity component anisotropy ratios
# Standard atmospheric boundary layer ratios:
#   v/u ≈ 0.8 (lateral component is 80% of longitudinal)
#   w/u ≈ 0.5 (vertical component is 50% of longitudinal)
turbulence_anisotropy_ratio_v = 0.8
turbulence_anisotropy_ratio_w = 0.5

# Mann Box asymmetry parameter α (dimensionless)
# Typical range: 0.8 - 2.0
# α = 1.0: Isotropic-like behavior
# α > 1.0: More anisotropic (typical for atmospheric flows)
turbulence_mann_asymmetry = 1.0

# Cross-spectrum coherence factors (dimensionless)
# Control the off-diagonal components of the spectral tensor
# Valid range: 0.0 - 1.0
turbulence_uv_coherence = 0.75  # u-v coherence (typical: 0.7-0.8)
turbulence_uw_coherence = 0.50  # u-w coherence (typical: 0.4-0.6)
turbulence_vw_coherence = 0.65  # v-w coherence (typical: 0.6-0.7)

################################################################################
# TERRAIN-AWARE MASKING
################################################################################

# Apply smooth masking to confine turbulence to fluid region (z > terrain)
enable_terrain_aware_masking = 1

# Transition height [m] from solid to fluid (typically 2-4 grid cells)
terrain_mask_transition_height = 50.0

################################################################################
# OUTPUT CONFIGURATION
################################################################################

# Extract wind at specific height AGL and write to CSV
extract_agl  = 15.0
extract_file = wind_extract_mann_box.csv

# Output AMReX plotfile
plot_file = plt_gaussian_hill_mann_box

# Output with turbulence fluctuations added
plot_file_with_fluctuations = plt_gaussian_hill_mann_box_with_fluctuations

################################################################################
# OPTIONAL: IEC 61400-1 PARAMETERS (for reference/validation)
################################################################################

# Wind turbine class (I, II, III, IV) for potential future coupling
# turbine_class = II

# Terrain category (0-4) for surface roughness
# terrain_category = 1

# Hub height [m] for wind turbine
# z_hub = 90.0

################################################################################
# NOTES FOR USERS
################################################################################

# To use different atmospheric stability conditions, modify these parameters:
#
# STABLE CONDITIONS (reduced turbulence):
#   turbulence_length_scale_u = 200.0
#   turbulence_anisotropy_ratio_v = 0.75  (more isotropic)
#   turbulence_intensity_ref = 0.08
#   turbulence_mann_asymmetry = 1.2
#
# UNSTABLE CONDITIONS (increased turbulence):
#   turbulence_length_scale_u = 400.0
#   turbulence_anisotropy_ratio_v = 0.85  (more anisotropic)
#   turbulence_intensity_ref = 0.16
#   turbulence_mann_asymmetry = 0.8
#
# WIND FARM WAKE CONDITIONS:
#   turbulence_length_scale_u = 250.0
#   turbulence_intensity_ref = 0.14
#   turbulence_mann_asymmetry = 1.1
#
# COMPLEX TERRAIN (highly variable flow):
#   turbulence_length_scale_u = 350.0
#   turbulence_intensity_ref = 0.15
#   turbulence_mann_asymmetry = 1.3
#   enable_terrain_aware_masking = 1

################################################################################
# END OF CONFIGURATION
################################################################################
