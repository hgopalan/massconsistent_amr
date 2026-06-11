# 3D Scalar Transport Test (Coupled Mode)
# Tests: Coupled mode running 3 time steps of coupled transport, using frozen wind after step 0
# Configuration: Flat terrain with uniform wind field and scalar transport

# Terrain file (3x3 grid, flat terrain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0

# Domain height [m] above maximum terrain elevation
domain_height = 200.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# ===================================================================
# 3D Scalar Transport Configuration
# ===================================================================

# Enable 3D scalar fields and transport equations
enable_3d_scalars = true
enable_temperature_transport = true
enable_moisture_transport = false

# Coupling Mode
scalar_coupling_mode = coupled
num_time_steps = 3

# Molecular diffusion coefficients [m²/s]
temperature_diffusivity = 2.5e-5
moisture_diffusivity = 2.2e-5

# Time stepping for scalar transport
# Use adaptive time stepping with CFL = 0.8
scalar_dt = -1.0
scalar_cfl = 0.8

# No multi-step correction for this simple test
multi_step_corrector_steps = 1

# ===================================================================
# Mixing Length Turbulence Model
# ===================================================================

# Enable mixing length model for eddy diffusivity enhancement
enable_mixing_length_turbulence = true

# Proportionality constant for mixing length
mixing_length_coefficient = 0.1

# Von Kármán constant
von_karman = 0.41

# Ground roughness for mixing length calculation [m]
zground = 0.1

# MLMG solver settings (verbose for diagnostics)
mlmg_verbose  = 1
max_grid_size = 32

# Extract wind at 50 m AGL
extract_agl  = 50.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_scalar_transport
