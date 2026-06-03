# Complete Synthetic Turbulence Example
# Full workflow from mean wind to OpenFAST turbulent field
# 
# This example demonstrates all five phases of the synthetic turbulence framework:
# Phase 1: Turbulence parameter configuration
# Phase 2: Random field synthesis (automatic)
# Phase 3: Time-series generation (automatic)
# Phase 4: Validation and testing (automatic)
# Phase 5: Documentation and visualization (post-processing)
#
# Usage:
#   ./build/wind_solver tutorials/example_synthetic_turbulence.i
#   python3 tools/bts_to_vtk.py turbulence_example.bts turbulence.vtk

# ============================================================================
# PART 1: MEAN WIND FIELD CONFIGURATION (Standard Solver)
# ============================================================================

# Terrain data (can be synthetic or from DEM file)
terrain_file = terrain.csv

# Reference wind conditions at measurement height
U_ref = 10.0    # [m/s] - mean wind speed
V_ref = 0.0     # [m/s] - lateral wind (0 = pure west wind)
z_ref = 10.0    # [m AGL] - standard meteorological measurement height

# Aerodynamic roughness length [m]
# Typical values:
#   z0 = 0.0001-0.001: Water/ice
#   z0 = 0.01-0.05: Grassland/sand
#   z0 = 0.1-0.5: Shrubs/forest
#   z0 = 0.5-2.0: Urban areas
z0 = 0.03

# ============================================================================
# GRID CONFIGURATION
# ============================================================================

# Horizontal grid spacing [m]
# Finer spacing → better terrain representation but longer computation
# Typical: 20-50 m for most applications
dx = 30.0
dy = 30.0

# Vertical grid spacing [m]
# Finer spacing near ground, but maintain uniform spacing for simplicity
# Typical: 10-50 m
dz = 25.0

# Domain height above maximum terrain [m]
# Should be 3-5× expected boundary layer height (typically 100-300 m)
domain_height = 100.0

# Anisotropy coefficients (Lagrange multiplier scaling)
# Typically set to 1.0 for most applications
alpha_h = 1.0    # Horizontal anisotropy
alpha_v = 1.0    # Vertical anisotropy

# ============================================================================
# SOLVER OPTIONS
# ============================================================================

# Verbosity level for MLMG solver
# 0 = silent, 1 = some output, 2 = verbose
mlmg_verbose = 0

# Maximum grid size for AMReX discretization
max_grid_size = 32

# ============================================================================
# WIND FIELD EXTRACTION (Optional diagnostic output)
# ============================================================================

# Extract wind field at specific AGL height
extract_agl = 15.0

# Write extracted wind field to CSV file
extract_file = wind_extract_example.csv

# ============================================================================
# PLOT OUTPUT
# ============================================================================

# AMReX plotfile prefix (creates .H, .D directories)
plot_file = plt_turbulence_example

# ============================================================================
# PHASE 1: SYNTHETIC TURBULENCE PARAMETERS
# ============================================================================

# Master enable flag
# Set to false to run standard mass-consistent solver without turbulence
enable_synthetic_turbulence = true

# --- Spectral Model Selection ---

# Spectrum model for turbulence generation
# Options:
#   VonKarman: Universal isotropic spectrum (default, recommended)
#   Kaimal: Empirical spectrum from IEC 61400-1 (wind energy standard)
turbulence_spectrum_model = VonKarman

# Turbulence intensity profile model
# Options:
#   PowerLaw: I(z) = I_ref * (z/z_ref)^α (standard in atmospheric science)
#   Logarithmic: I(z) = I_ref * ln(z/z₀) / ln(z_ref/z₀) (surface layer theory)
#   Constant: I(z) = constant (simplified)
turbulence_intensity_model = PowerLaw

# Coherence decay model
# Options:
#   Gaussian: ρ(Δy) = exp(-(k*Δy)²) (smooth decay, default)
#   Exponential: ρ(Δy) = exp(-k*Δy) (faster decay)
turbulence_coherence_model = Gaussian

# --- Turbulence Intensity Configuration ---

# Reference turbulence intensity [fraction, dimensionless]
# Represents standard deviation of wind speed relative to mean
# Typical ranges:
#   0.06-0.10: Smooth surfaces (water)
#   0.10-0.15: Open terrain (grass, farmland)
#   0.15-0.25: Complex terrain (hills, scattered buildings)
#   0.20-0.30: Urban areas, forests
turbulence_intensity_ref = 0.14

# Reference height for turbulence intensity [m AGL]
# Standard meteorological measurement height
turbulence_z_intensity_ref = 10.0

# Power-law exponent for intensity variation with height
# Used when turbulence_intensity_model = PowerLaw
# I(z) = I_ref * (z/z_ref)^α
# Typical ranges:
#   0.10-0.15: Stable conditions, smooth terrain
#   0.15-0.20: Neutral conditions, gentle terrain
#   0.20-0.30: Unstable conditions, complex terrain
turbulence_intensity_exponent = 0.14

# --- Integral Length Scale Configuration ---

# Longitudinal integral length scale [m]
# Also called "u-component" or "streamwise" length scale
# Characteristic size of largest eddies in wind direction
# Typical range: 100-500 m (height and stability dependent)
# Larger values: more coherent, longer-lasting structures
turbulence_length_scale_u = 300.0

# Lateral integral length scale [m]
# Also called "v-component" or "crosswind" length scale
# Typical ratio to u: 0.6-0.8 (weaker correlations laterally)
turbulence_length_scale_v = 200.0

# Vertical integral length scale [m]
# Also called "w-component" length scale
# Typical ratio to u: 0.3-0.5 (much weaker vertical correlations)
turbulence_length_scale_w = 120.0

# --- Coherence Decay Configuration ---

# Vertical coherence decay factor [1/m]
# Controls how quickly spatial correlations decay with height difference
# ρ(Δz) ≈ exp(-a*Δz) for exponential, exp(-(a*Δz)²) for Gaussian
# Typical range: 0.006-0.010 (higher = faster decay)
turbulence_coherence_decay_vertical = 0.008

# Lateral (horizontal) coherence decay factor [1/m]
# Controls decay of correlations perpendicular to mean wind
# Typical range: 0.004-0.008
turbulence_coherence_decay_lateral = 0.006

# --- Anisotropy Ratios ---

# Ratio of lateral (v) to longitudinal (u) velocity RMS
# Standard deviation ratio: σ_v / σ_u
# Typical: 0.75-0.85 (lateral turbulence weaker than longitudinal)
# Represents physical asymmetry of atmospheric turbulence
turbulence_anisotropy_ratio_v = 0.80

# Ratio of vertical (w) to longitudinal (u) velocity RMS
# Standard deviation ratio: σ_w / σ_u
# Typical: 0.45-0.55 (vertical turbulence much weaker)
# Strong anisotropy reflects suppression of vertical mixing near surface
turbulence_anisotropy_ratio_w = 0.50

# ============================================================================
# PHASE 2: RANDOM FIELD GENERATION (Automatic, no additional parameters)
# ============================================================================

# Random seed for reproducible field generation
# Same seed produces identical turbulence fields
# Useful for sensitivity studies and testing
# Value: any unsigned integer (suggested: 12345-99999)
turbulence_random_seed = 42

# ============================================================================
# PHASE 3: TIME-SERIES & EXPORT (Automatic post-processing)
# ============================================================================

# Export format specification
# Currently supported: bts (TurbSim binary format)
# Other formats planned: netcdf, hdf5
turbulence_export_format = bts

# Output filename for turbulence field
# Will create:
#   - turbulence_example.bts (binary, machine-readable)
#   - turbulence_example.bts.meta (ASCII, human-readable metadata)
turbulence_output_file = turbulence_example.bts

# ============================================================================
# PHASE 4: VALIDATION (Automatic post-processing)
# ============================================================================

# Validation is automatic; no configuration needed
# Tests include:
# - Energy conservation (Parseval's theorem)
# - Mass continuity (∇·u' ≈ 0)
# - Anisotropy ratios
# - Integral scale recovery
# - Coherence decay
# - OpenFAST format compliance

# ============================================================================
# PHASE 5: DOCUMENTATION & VISUALIZATION (Post-processing)
# ============================================================================

# To convert BTS to VTK format for ParaView visualization:
#   python3 tools/bts_to_vtk.py turbulence_example.bts turbulence.vtk
#
# To create time-series VTK files (one per time step):
#   python3 tools/bts_to_vtk.py turbulence_example.bts output --time-series
#
# Then open output.pvd in ParaView to animate the time series
# See tutorials/PHASE5_TUTORIAL_SYNTHETIC_TURBULENCE.md for detailed instructions
