# Single Column Model (SCM) — Neutral Flat-Terrain Regression Test
# Modified: 10 m/s @ 80 m height, latitude 45°, z0=0.1
# Date: 2026-07-01

# --- Terrain ------------------------------------------------------------------
terrain_file = terrain.csv

# --- Initialization mode ------------------------------------------------------
init_mode = scm

# --- SCM parameters -----------------------------------------------------------
# Near-neutral conditions: large positive L_obukhov
scm.U_ref      = 10.0      # reference wind speed [m/s]
scm.dir_ref    = 270.0     # meteorological direction (westerly)
scm.z_ref      = 80.0      # reference height [m] - MODIFIED FROM 10.0
scm.z0         = 0.1       # roughness length [m]
scm.L_obukhov  = 1.0e6    # Monin-Obukhov length (neutral)
scm.latitude   = 45.0      # latitude [deg]
scm.T_ref      = 300.0     # reference potential temperature [K]
scm.z_T_ref    = 2.0       # height of T_ref measurement [m]
scm.lapse_rate = 0.003     # free-troposphere lapse rate [K/m]
scm.dt         = 60.0      # time step [s]
scm.max_time   = 86400.0   # 24 h spin-up for CI speed
scm.conv_tol   = 1.0e-4    # relaxed tolerance for regression test

# --- 3D grid (small domain for fast CI) ---------------------------------------
dx = 50.0
dy = 50.0
dz = 50.0
domain_height  = 500.0     # 10 vertical levels

# --- Solver anisotropy coefficients -------------------------------------------
alpha_h = 1.0
alpha_v = 1.0

# --- MLMG settings ------------------------------------------------------------
mlmg_verbose  = 0
max_grid_size = 32

# --- Output -------------------------------------------------------------------
extract_agl  = 15.0
extract_file = wind_extract_80m.csv
plot_file    = plt_scm_neutral_80m_resistancelaw

scm.mode = resistancelaw
