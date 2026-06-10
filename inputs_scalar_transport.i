# Example input file demonstrating 3D scalar transport features
# Temperature and moisture transport equations with mixing length turbulence model

# Terrain and domain setup
terrain_file = terrain.csv
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1
dx = 30.0
dy = 30.0
dz = 30.0
domain_height = 500.0

# Anisotropy factors
alpha_h = 1.0
alpha_v = 1.0

# Solver parameters
mlmg_verbose = 1
max_grid_size = 32
plot_file = plt_scalar_transport

# ===================================================================
# 3D Scalar Transport Configuration
# ===================================================================

# Enable 3D scalar fields (temperature and/or moisture)
# - enable_3d_scalars: Master switch (automatically enabled if transport is enabled)
# - enable_temperature_transport: Solve temperature transport equation
# - enable_moisture_transport: Solve moisture transport equation
enable_3d_scalars = true
enable_temperature_transport = true
enable_moisture_transport = false

# Molecular diffusion coefficients [m²/s]
# - temperature_diffusivity: Thermal diffusivity (default: 2.5e-5 m²/s)
# - moisture_diffusivity: Moisture diffusivity (default: 2.2e-5 m²/s)
temperature_diffusivity = 2.5e-5
moisture_diffusivity = 2.2e-5

# Time stepping for scalar transport
# - scalar_dt: Fixed time step (negative = auto-compute via CFL)
# - scalar_cfl: CFL number for adaptive time stepping (default: 0.8)
scalar_dt = -1.0
scalar_cfl = 0.8

# Multi-step corrector iterations for temperature-dependent wind field
# - If temperature significantly affects buoyancy, use > 1 for tighter coupling
#   This allows the mass-consistent solver to re-solve with updated temperature RHS
multi_step_corrector_steps = 1

# ===================================================================
# Mixing Length Turbulence Model for Eddy Diffusivity
# ===================================================================
# 
# The mixing length model computes eddy diffusivity as:
#   K_eddy = (l_m)^2 * |∇u|
# where l_m = von_karman * (z + z0) * mixing_length_coefficient
#
# This enhances molecular diffusion: K_eff = K_mol + K_eddy
# Enables more realistic turbulent mixing in scalars.

# Enable mixing length model for eddy diffusivity enhancement
enable_mixing_length_turbulence = true

# Proportionality constant for mixing length calculation
# l_m = von_karman * (z + z0) * mixing_length_coefficient
# Typical value: 0.1 (relates to distance from surface)
mixing_length_coefficient = 0.1

# Von Kármán constant (universal, typically 0.41)
von_karman = 0.41

# Ground roughness for mixing length calculation [m]
# (used in l_m = von_karman * (z + z0) * mixing_length_coefficient)
zground = 0.1
