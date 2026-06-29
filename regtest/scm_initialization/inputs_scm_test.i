# SCM (Single Column Model) Initialization Test
# Based on Python reference: https://github.com/hgopalan/onedterrainsolver/blob/main/hrrr_1dsolver_terrain.py
# Test Case: MOL=-1e30 (neutral stability)
# Met Mast Height: 150m, Target Wind: [10, 0] m/s
# This matches the reference test case in the Python code

# Flat terrain for this idealized test
terrain_file = terrain.csv 

# SCM initialization mode
init_mode = scm

# Reference wind at z_ref (to match test case)
# Python: metMastWind=[10,0] at metMastHeight=150
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0  # Log-law reference (not used for SCM, but kept for completeness)
z0 = 0.1

# Domain and grid - small domain for quick test
dx = 20.0
dy = 20.0
dz = 20.0
domain_height = 500.0
alpha_h = 1.0
alpha_v = 1.0

# Grid sizing (small for fast regression testing)
max_grid_size = 32

# Solver parameters
mlmg_verbose = 0
tol_rel = 1.e-8

# Output files
plot_file = plt_scm_test
extract_agl = 150.0
extract_file = scm_extract.csv

# ============================================================================
# SCM-specific parameters
# ============================================================================

# 1D SCM grid parameters
# Python: npts=201, zheight=2000, dz=(2000-0)/200=10m (our approximation: dz=4m)
scm_height = 2000.0        # Max height for 1D SCM (m)
scm_dz = 4.0               # Grid spacing for 1D SCM (m)

# Reference height for convergence criterion
# Python: metMastHeight=150m
scm_z_ref = 150.0

# Latitude for Coriolis parameter
# Python: coriolis=45 (latitude)
scm_latitude = 45.0

# Heat flux mode: 1=heat_flux, 2=surface_temp, 3=heating_rate, 4=MOL
# Python test case: heat_flux_mode=4, mol_length=MOL[i] where MOL[i]=-1e30 (neutral)
scm_heat_flux_mode = 4    # MOL specified

# Value for specified heat flux mode
# For mode 4 (MOL): use a large negative value for neutral conditions
scm_heat_flux_value = -1.0e30

# Reference temperature (K)
# Python: air_temp (used in test case, let's use typical value)
scm_temperature_reference = 300.0

# Surface temperature (only used if scm_heat_flux_mode==2)
scm_temperature_surface = 300.0

# Convergence tolerance for geostrophic wind iteration (m/s)
# Python: allowed_error=0.25
scm_convergence_tolerance = 0.25

# Simulation time for single SCM run (s)
# Python: num_of_steps=20000 (time stepping, converted to seconds)
scm_simulation_time = 20000.0

# Maximum geostrophic wind iterations
# Python implicit in generate_profile loop
scm_max_iterations = 100

# ============================================================================
# Wake model disabled for pure initialization test
# ============================================================================
enable_wake = false

# ============================================================================
# All other features disabled to focus on initialization
# ============================================================================
enable_canopy = false
enable_buildings = false
enable_dispersion = false
enable_temperature_transport = false
enable_moisture_transport = false
