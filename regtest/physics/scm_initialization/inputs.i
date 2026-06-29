# Regression test for Single Column Model (SCM) wind profile initialization
#
# Reference case matching hrrr_1dsolver_terrain.py (hgopalan/onedterrainsolver):
#   MOL = 500 (effectively neutral convergence)
#   metMastHeight  = 150 m
#   metMastWind    = [10, 0] m/s  (eastward, 10 m/s)
#   z0             = 0.1 m
#   latitude       = 45°
#   domain height  = 1000 m, nz = 101, dz = 10 m
#   lapse_rate     = 0.01 K/m
#   initial guess  : Ug0 = 10 m/s, Vg0 = -12 m/s
#
# Expected outputs (from hrrr_1dsolver_terrain.py):
#   Monin-Obukhov Length  : -1e+30 (neutral sentinel)
#   Richardson BL Height  : 800.0 m
#   Friction Velocity     : 0.6337 m/s
#   Geostrophic Wind      : [13.9206, -10.3659] m/s
#   CFD Met Mast Wind     : [9.83213, 0.110555] m/s
#   Wind Error            : [-0.167866, 0.110555] m/s (both < 0.25 m/s)

# ============================================================================
# GEOMETRY
# ============================================================================
geometry.prob_lo = 0.0 0.0 0.0
geometry.prob_hi = 1000.0 1000.0 1000.0  # Domain height = 1 km for SCM
geometry.is_periodic = 1 1 0

# ============================================================================
# GRID
# ============================================================================
amr.n_cell = 16 16 100         # 100 cells × dz=10 m = 1000 m
amr.max_level = 0

# ============================================================================
# WIND INITIALIZATION - SINGLE COLUMN MODEL (SCM) MODE
# ============================================================================
init_mode = scm

# Target wind at reference (met mast) height
scm_wind_speed     = 10.0      # Wind speed at reference height [m/s]
scm_wind_direction = 0.0       # Wind direction: 0° → ux=10, uy=0 (eastward)
scm_ref_height     = 150.0     # Met mast height [m AGL]

# Thermodynamic profile
scm_ref_temperature = 288.15   # Surface temperature [K] (15°C)
scm_lapse_rate      = 0.01     # Dry lapse rate [K/m] (matches Python reference)

# 1D SCM column parameters (matches Python npts=101, zheight=1000)
scm_domain_height = 1000.0     # SCM column height [m]
scm_dz            = 10.0       # SCM vertical resolution [m]  (101 levels)

# Initial geostrophic wind guess for outer iteration.
# Starting from (10, -12) matches the Python reference initial conditions and
# significantly speeds convergence for this NH latitude-45° case.
scm_initial_ug = 10.0          # Initial Ug guess [m/s]
scm_initial_vg = -12.0         # Initial Vg guess [m/s]

# ============================================================================
# SURFACE PARAMETERS
# ============================================================================
z0       = 0.1                 # Aerodynamic roughness length [m]
latitude = 45.0                # Latitude [degrees]

# ============================================================================
# SOLVER PARAMETERS
# ============================================================================
mlmg.verbose  = 1
mlmg.max_iter = 100
mlmg.tol_rel  = 1.0e-6
mlmg.tol_abs  = 1.0e-8

# ============================================================================
# OUTPUT
# ============================================================================
plot_file  = plt_scm
extract_agl = 150.0            # Extract wind at reference height [m AGL]
