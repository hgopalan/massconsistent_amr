# Data Center Heat Island - Multiple Facilities Test
# Tests support for multiple simultaneous data center heat sources
# Expected: Multiple thermal plumes rise and interact

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
x_max = 4000.0
y_max = 4000.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable 3D temperature transport
enable_3d_scalars = true
enable_temperature_transport = true
temperature_diffusivity = 2.5e-5
scalar_cfl = 0.8

# Multiple data center heat sources
datacenter.enabled = true

# Three facilities: array format
datacenter.heat_release = 1.0e7 5.0e6 8.0e6
datacenter.x = 1000.0 1500.0 2500.0
datacenter.y = 1000.0 2000.0 1500.0
datacenter.z = 10.0 15.0 12.0
datacenter.area = 10000.0 5000.0 8000.0
datacenter.sigma_x = 100.0 75.0 90.0
datacenter.sigma_y = 100.0 75.0 90.0
datacenter.sigma_z = 10.0 8.0 9.0
datacenter.names = "DataCenter_A" "DataCenter_B" "DataCenter_C"

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
plot_file = plt_datacenter_multi
num_time_steps = 1
