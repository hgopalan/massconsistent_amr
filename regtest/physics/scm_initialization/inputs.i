# Regression test for Single Column Model (SCM) wind profile initialization
# This test validates the SCM's ability to find geostrophic wind from a specified wind speed at a reference height

# ============================================================================
# GEOMETRY
# ============================================================================
geometry.prob_lo = 0.0 0.0 0.0
geometry.prob_hi = 1000.0 1000.0 4000.0  # Domain height = 4 km for SCM
geometry.is_periodic = 1 1 0

# ============================================================================
# GRID
# ============================================================================
amr.n_cell = 16 16 1000
amr.max_level = 0

# ============================================================================
# WIND INITIALIZATION - SINGLE COLUMN MODEL (SCM) MODE
# ============================================================================
# Use SCM mode to find geostrophic wind from specified wind speed at height
init_mode = scm

# SCM Parameters
scm_wind_speed = 10.0           # Wind speed at reference height [m/s]
scm_wind_direction = 270.0      # Wind direction [degrees, 0=N, 90=E, 180=S, 270=W]
scm_ref_height = 10.0           # Height where wind speed is specified [m AGL]
scm_ref_temperature = 288.15    # Reference temperature at surface [K] (15°C)
scm_lapse_rate = 0.0065         # Temperature lapse rate [K/m] (standard atmosphere)
scm_domain_height = 4000.0      # Domain height for 1D SCM [m]
scm_dz = 4.0                    # Vertical resolution for 1D SCM [m]

# Optional stability parameters (uncomment to use non-neutral conditions)
# scm_heat_flux = 100.0                 # Surface sensible heat flux [W/m^2] (positive=unstable)
# scm_monin_obukhov_length = 50.0       # Monin-Obukhov length [m] (positive=stable, negative=unstable)

# ============================================================================
# SURFACE PARAMETERS
# ============================================================================
z0 = 0.1                        # Aerodynamic roughness length [m]
latitude = 45.0                 # Latitude [degrees]

# ============================================================================
# 3D SCALAR TRANSPORT (Temperature and Moisture)
# ============================================================================
enable_3d_scalars = true        # Enable 3D temperature field
enable_temperature_transport = true  # Evolve temperature with scalar transport

# ============================================================================
# SOLVER PARAMETERS
# ============================================================================
mlmg.verbose = 1
mlmg.max_iter = 100
mlmg.tol_rel = 1.0e-6
mlmg.tol_abs = 1.0e-8

# ============================================================================
# OUTPUT
# ============================================================================
plot_file = plt_scm
extract_agl = 10.0              # Extract wind at 10m AGL
