# Regression test for SCM Non-Canonical Boundary Layers
# This test exercises new non-canonical boundary layer capabilities:
# - Convective Boundary Layer (sensible heat flux)
# - Stable Boundary Layer (surface cooling rate)
# - Charnock overwater roughness length
# - Subsidence advection
# - Radiative cooling

# ============================================================================
# GEOMETRY
# ============================================================================
geometry.prob_lo = 0.0 0.0 0.0
geometry.prob_hi = 1000.0 1000.0 4000.0
geometry.is_periodic = 1 1 0

# ============================================================================
# GRID
# ============================================================================
amr.n_cell = 8 8 100
amr.max_level = 0

# ============================================================================
# WIND INITIALIZATION
# ============================================================================
init_mode = scm

# SCM Parameters
scm_wind_speed = 12.0
scm_wind_direction = 240.0
scm_ref_height = 20.0
scm_ref_temperature = 285.0
scm_lapse_rate = 0.005
scm_domain_height = 4000.0
scm_dz = 40.0
scm_turbulence_model = ysu

# Non-canonical features
scm_heat_flux = 200.0
scm_monin_obukhov_length = -50.0

# Query scm namespace features
scm.cooling_rate = 2.0
scm.use_charnock = true
scm.subsidence_rate = 0.01
scm.radiation_cooling_rate = 1.0

# ============================================================================
# SURFACE PARAMETERS
# ============================================================================
z0 = 0.01
latitude = 60.0

# ============================================================================
# 3D SCALAR TRANSPORT
# ============================================================================
enable_3d_scalars = true
enable_temperature_transport = true

# ============================================================================
# OUTPUT
# ============================================================================
plot_file = plt_scm_non_canonical
extract_agl = 20.0
