# Single Column Model (SCM) — Stably Stratified Flat-Terrain Regression Test
#
# Verifies:
#   1. Successful SCM initialization with init_mode = scm under stable conditions
#   2. Reduced boundary layer height (stable ABL shallower than neutral)
#   3. Low-level jet structure: wind speed maximum near top of stable layer
#   4. Positive temperature gradient (θ increasing with z) throughout the column
#   5. Suppressed TKE relative to neutral case: max(e) < neutral max(e)
#   6. Terrain-aware assignment: u=v=0 inside terrain (z_agl <= 0)
#
# Stability: L_obukhov = +50 m (strongly stable)
# Expected ABL depth ~ u*/(f*N) ≈ 200-400 m for these conditions
#
# Reference: Maronga et al. (2015, Geosci. Model Dev.), Deardorff (1980),
#            Zilitinkevich (1972), Clarke & Hess (1974)
# Date: 2026-07-01

# --- Terrain ------------------------------------------------------------------
terrain_file = terrain.csv

# --- Initialization mode ------------------------------------------------------
init_mode = scm

# --- SCM parameters (stable stratification) -----------------------------------
# Positive L_obukhov: buoyancy suppresses turbulence
scm.U_ref      = 8.0       # reference wind speed [m/s]
scm.dir_ref    = 270.0     # meteorological direction (westerly)
scm.z_ref      = 10.0      # reference height [m]
scm.z0         = 0.1       # roughness length [m]
scm.L_obukhov  = 50.0      # Monin-Obukhov length [m]; +ve = stable
scm.latitude   = 45.0      # latitude [deg]
scm.T_ref      = 300.0     # reference potential temperature [K]
scm.z_T_ref    = 2.0       # height of T_ref measurement [m]
scm.lapse_rate = 0.005     # stronger free-troposphere lapse rate [K/m]
scm.dt         = 60.0      # time step [s]
scm.max_time   = 86400.0   # 24 h spin-up for CI speed
scm.conv_tol   = 1.0e-4    # relaxed tolerance for regression test

# --- 3D grid (small domain for fast CI) ---------------------------------------
dx = 50.0
dy = 50.0
dz = 50.0
domain_height  = 500.0

# --- Solver anisotropy coefficients -------------------------------------------
alpha_h = 1.0
alpha_v = 1.0

# --- MLMG settings ------------------------------------------------------------
mlmg_verbose  = 0
max_grid_size = 32

# --- Output -------------------------------------------------------------------
extract_agl  = 15.0
extract_file = wind_extract.csv
plot_file    = plt_scm_stable
