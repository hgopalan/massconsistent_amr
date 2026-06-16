# Dense Gas Dispersion - CO₂ Release on Terrain
# Test case for SLAB/UGC model with terrain interaction
# Domain: 500m x 500m x 100m, Gaussian hill terrain (peak 20 m at center)
# Source: 1.0 kg/s CO₂ release on hill at 25 m height
# Wind: 10 m/s from west
# Expected: Dense gas layer height and concentration interaction with terrain

# Enable puff model and dense gas
enable_puff = true
enable_dense_gas = true
enable_terrain_reflection = true

# Source location and emission
source_x  = 250.0       # Source x-coordinate [m]
source_y  = 250.0       # Source y-coordinate [m]
source_z  = 25.0        # Source height [m] (on hill)
emission_rate = 1.0     # Emission rate [kg/s]
emission_duration = 100.0  # Duration of emission [s]

# Dense gas species parameters (CO₂)
gas_molecular_weight = 44.01    # CO₂ molar mass [g/mol]
gas_density = 1.98              # CO₂ density at std conditions [kg/m³]
initial_layer_height = 8.0      # Initial SLAB layer height H₀ [m]
slab_decay_scale = 200.0        # Characteristic decay scale x_max [m]
slab_power_exponent = 0.667     # SLAB height decay exponent (2/3)
lateral_spreading_coeff = 0.15  # Lateral spreading coefficient
entrainment_coefficient = 0.1   # Vertical mixing entrainment

# Diffusivity parameters
K_h = 1.0               # Horizontal diffusivity [m²/s]
K_v = 0.5               # Vertical diffusivity [m²/s]

# Initial puff size
sigma_y0 = 3.0          # Initial lateral width [m]
sigma_z0 = 3.0          # Initial vertical height [m]

# Wind field
U_wind = 10.0           # x-component of wind [m/s]
V_wind = 0.0            # y-component of wind [m/s]
W_wind = 0.0            # z-component of wind [m/s]

# Domain extent
xmin = 0.0              # Domain min x [m]
xmax = 500.0            # Domain max x [m]
ymin = 0.0              # Domain min y [m]
ymax = 500.0            # Domain max y [m]
zmin = 0.0              # Domain min z [m]
zmax = 100.0            # Domain max z [m]

# Concentration grid resolution
dx = 10.0               # Grid spacing x [m]
dy = 10.0               # Grid spacing y [m]
dz = 2.0                # Fine vertical resolution [m]

# Time stepping
dt_puff = 0.5           # Time step [s]
n_steps_puff = 200      # Number of time steps (total time = 100 s)
output_freq_puff = 20   # Write concentration every 20 steps

# Output file
puff_output = puff_dense_gas_terrain.csv
