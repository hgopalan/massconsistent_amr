# Regression test for SCM Cloud-Top Radiative Cooling
# This test validates dynamic cloud-top cooling linked with microphysics:
# - Microphysics enabled with 95% initial relative humidity to form clouds
# - Cloud-top cooling rate specified to cool the cloud-top boundary dynamically

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
scm_wind_speed = 5.0
scm_wind_direction = 270.0
scm_ref_height = 10.0
scm_ref_temperature = 280.0
scm_lapse_rate = 0.008
scm_domain_height = 4000.0
scm_dz = 40.0
scm_turbulence_model = ysu

# Microphysics and humidity to form clouds
scm_enable_microphysics = true
scm_initial_humidity = 0.95

# Cloud-top radiative cooling
scm.cloud_top_cooling_rate = 3.0

# ============================================================================
# SURFACE PARAMETERS
# ============================================================================
z0 = 0.1
latitude = 45.0

# ============================================================================
# 3D SCALAR TRANSPORT
# ============================================================================
enable_3d_scalars = true
enable_temperature_transport = true

# ============================================================================
# OUTPUT
# ============================================================================
plot_file = plt_scm_cloud_top_cooling
extract_agl = 10.0
