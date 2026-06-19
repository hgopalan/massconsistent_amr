# Data Center Heat Island - Flat Terrain Test
# Simple validation case: data center heat source on flat ground
# Expected: Thermal plume rises and disperses downwind

# Terrain - flat ground
terrain_file = terrain_flat.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.05

# Grid spacing [m]
dx = 25.0
dy = 25.0
dz = 20.0

# Domain size [m]
domain_height = 300.0
x_max = 3000.0
y_max = 3000.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable 3D temperature transport
enable_3d_scalars = true
enable_temperature_transport = true
temperature_diffusivity = 2.5e-5
scalar_cfl = 0.8

# Data center heat source
datacenter.enabled = true
datacenter.heat_release = 1.0e7
datacenter.x = 1500.0
datacenter.y = 1500.0
datacenter.z = 10.0
datacenter.area = 10000.0
datacenter.sigma_x = 100.0
datacenter.sigma_y = 100.0
datacenter.sigma_z = 10.0

# Temperature profile (neutral stratification)
temperature_file = temperature.csv
temperature_reference = 300.0

# MLMG solver settings
mlmg_verbose = 0
max_grid_size = 32

# Extract wind at 50 m AGL
extract_agl = 50.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_datacenter_flat
num_time_steps = 1
