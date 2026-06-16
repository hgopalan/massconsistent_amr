# Dense Gas Dispersion - HF Release with Reactive Chemistry
# Test case for SLAB/UGC model with reactive chemistry (HF photodissociation)
# Domain: 500m x 500m x 100m, flat terrain
# Source: 0.5 kg/s HF release at ground level (denser than CO₂)
# Wind: 8 m/s from west
# Expected: More pronounced gravity spreading than CO₂ due to higher density ratio

# Enable puff model and dense gas
enable_puff = true
enable_dense_gas = true
enable_chemistry = false  # HF doesn't decay as rapidly as NO₂/SO₂

# Source location and emission
source_x  = 250.0       # Source x-coordinate [m]
source_y  = 250.0       # Source y-coordinate [m]
source_z  = 0.5         # Source height [m] (ground release)
emission_rate = 0.5     # Emission rate [kg/s]
emission_duration = 100.0  # Duration of emission [s]

# Dense gas species parameters (HF - hydrogen fluoride)
gas_molecular_weight = 20.01    # HF molar mass [g/mol]
gas_density = 0.927             # HF density at std conditions [kg/m³]
initial_layer_height = 3.0      # Initial SLAB layer height H₀ [m] (lower than CO₂)
slab_decay_scale = 150.0        # Characteristic decay scale x_max [m]
slab_power_exponent = 0.667     # SLAB height decay exponent (2/3)
lateral_spreading_coeff = 0.15  # Lateral spreading coefficient
entrainment_coefficient = 0.1   # Vertical mixing entrainment

# Diffusivity parameters
K_h = 0.5               # Horizontal diffusivity [m²/s]
K_v = 0.3               # Vertical diffusivity [m²/s]

# Initial puff size
sigma_y0 = 2.0          # Initial lateral width [m]
sigma_z0 = 2.0          # Initial vertical height [m]

# Wind field (moderate wind)
U_wind = 8.0            # x-component of wind [m/s]
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
puff_output = puff_dense_gas_hf.csv
