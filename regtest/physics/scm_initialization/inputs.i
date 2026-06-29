# Regression test for Single Column Model (SCM) wind profile initialization.
# This case mirrors the reference hrrr_1dsolver_terrain.py setup:
# - met mast height = 150 m
# - met mast wind = [10, -4] m/s
# - Monin-Obukhov length = 500 m

# ============================================================================
# GEOMETRY
# ============================================================================
geometry.prob_lo = 0.0 0.0 0.0
geometry.prob_hi = 1000.0 1000.0 1000.0  # Domain height = 1 km for the reference case
geometry.is_periodic = 1 1 0

# ============================================================================
# GRID
# ============================================================================
amr.n_cell = 16 16 100
amr.max_level = 0

# ============================================================================
# WIND INITIALIZATION - SINGLE COLUMN MODEL (SCM) MODE
# ============================================================================
# Use SCM mode to find geostrophic wind from specified wind speed at height
init_mode = scm

# SCM Parameters
scm_wind_speed = 10.7703296143   # sqrt(10^2 + (-4)^2) from the reference met mast wind [10, -4] m/s
scm_wind_direction = 111.8014094863  # atan2(10, -4) in meteorological convention, from which the wind blows [deg]
scm_ref_height = 150.0           # Height where wind speed is specified [m AGL]
scm_ref_temperature = 300.0      # Reference temperature at surface [K]
scm_lapse_rate = 0.01            # Temperature lapse rate [K/m]
scm_domain_height = 1000.0       # Domain height for 1D SCM [m]
scm_dz = 10.0                   # Vertical resolution for 1D SCM [m]

# Optional stability parameters for the reference case
scm_monin_obukhov_length = 500.0  # Monin-Obukhov length [m]

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
extract_agl = 150.0              # Extract wind at 150m AGL
