# Lagrangian Particle Dispersion Model (LPDM) - Drift Correction and Spatial Capping Lid Test
# Tests ground reflection, drift correction with height-dependent diffusivity, and spatially-varying capping lid
# Domain: 300m x 300m x 200m

enable_puff = false
enable_lpdm = true
particles_per_step = 10
lpdm_random_seed = 42

source_x  = 100.0      # Source x-coordinate [m] (upwind)
source_y  = 150.0      # Source y-coordinate [m] (center)
source_z  = 20.0       # Source height above sea level [m]
emission_rate = 1.0    # Emission rate [units/s]
emission_duration = 50.0  # Duration of emission [s]

# Diffusivity parameters
K_h = 1.0              # Horizontal diffusivity [m²/s]
K_v = 0.5              # Vertical diffusivity [m²/s]

# Enable height-dependent K to trigger drift correction
enable_height_dependent_K = true
K_profile = power_law
K_power_law_exponent = 0.5
K_reference_height = 10.0

# Wind field (uniform for this test)
U_wind = 10.0          # x-component of wind [m/s]
V_wind = 0.0           # y-component of wind [m/s]
W_wind = 0.0           # z-component of wind [m/s]

# Terrain awareness
terrain_file = terrain.csv
enable_terrain_reflection = true

# Spatially-varying capping lid (mixing depth)
enable_capping_lid = true
capping_lid_file = capping_lid.csv

# Domain extent
xmin = 0.0
xmax = 300.0
ymin = 0.0
ymax = 300.0
zmin = 0.0
zmax = 200.0

# Concentration grid resolution
dx = 15.0
dy = 15.0
dz = 10.0

# Time stepping
dt_puff = 0.5
n_steps_puff = 100
output_freq_puff = 20

# Output file
puff_output = lpdm_concentration_drift_capping.csv
