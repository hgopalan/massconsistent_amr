# Data Center Heat Island - Valley Terrain Test
# Moderately complex terrain: narrow valley with data center on valley floor
# Expected: Plume rise and thermal circulation interacting with valley geometry

# Terrain file (valley geometry)
terrain_file = terrain_valley.csv

# Log-law initialization
init_mode = loglaw
U_ref = 8.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 25.0

# Domain size [m]
domain_height = 400.0
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

# Data center heat source (on valley floor)
datacenter.enabled = true
datacenter.heat_release = 5.0e7
datacenter.x = 2000.0
datacenter.y = 2000.0
datacenter.z = 150.0
datacenter.area = 15000.0
datacenter.sigma_x = 120.0
datacenter.sigma_y = 120.0
datacenter.sigma_z = 15.0

# Temperature profile (stable stratification above)
temperature_file = temperature_stable.csv
temperature_reference = 300.0

# Kinematic terrain BC
enable_terrain_kinematic_bc = true

# MLMG solver settings
mlmg_verbose = 0
max_grid_size = 32

# Extract wind at 100 m AGL
extract_agl = 100.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_datacenter_valley
num_time_steps = 1
