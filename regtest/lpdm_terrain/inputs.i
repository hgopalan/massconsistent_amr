# Lagrangian Particle Dispersion Model (LPDM) - Terrain Awareness Test
# Tests ground reflection over Gaussian hill terrain
# Domain: 300m x 300m x 100m
# Source: upwind of hill at 10m height
# Wind: 10 m/s from west (positive x-direction)

# Enable LPDM model, disable puff
enable_puff = false
enable_lpdm = true
particles_per_step = 50
lpdm_random_seed = 42

# Source location and emission (upwind of hill center)
source_x  = 100.0      # Source x-coordinate [m] (upwind)
source_y  = 150.0      # Source y-coordinate [m] (center)
source_z  = 20.0       # Source height above sea level [m]
emission_rate = 1.0    # Emission rate [units/s]
emission_duration = 50.0  # Duration of emission [s]

# Diffusivity parameters
K_h = 1.0              # Horizontal diffusivity [m²/s]
K_v = 0.5              # Vertical diffusivity [m²/s]

# Wind field (uniform for this test)
U_wind = 10.0          # x-component of wind [m/s]
V_wind = 0.0           # y-component of wind [m/s]
W_wind = 0.0           # z-component of wind [m/s]

# Terrain awareness
terrain_file = terrain.csv           # Gaussian hill terrain
enable_terrain_reflection = true    # Enable ground reflection

# Domain extent
xmin = 0.0             # Domain min x [m]
xmax = 300.0           # Domain max x [m]
ymin = 0.0             # Domain min y [m]
ymax = 300.0           # Domain max y [m]
zmin = 0.0             # Domain min z [m]
zmax = 100.0           # Domain max z [m]

# Concentration grid resolution
dx = 15.0              # Grid spacing x [m]
dy = 15.0              # Grid spacing y [m]
dz = 10.0              # Grid spacing z [m]

# Time stepping
dt_puff = 0.5          # Time step [s]
n_steps_puff = 100     # Number of time steps (total time = 50 s)
output_freq_puff = 20  # Write concentration every 20 steps

# Output file
puff_output = lpdm_concentration_terrain.csv  # Output file prefix
