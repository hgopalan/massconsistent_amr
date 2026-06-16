# Dense Gas Dispersion - CO₂ Release (ρ_gas > ρ_air)
# Test case for SLAB/UGC-based hazardous material dispersion modeling
# Domain: 500m x 500m x 100m, flat terrain
# Source: 100 kg/s CO₂ release at ground level (heavy gas, sinks and spreads)
# Wind: 5 m/s from west (lower wind speed emphasizes gravity effects)

# Enable puff model
enable_puff = true

# Enable dense gas dispersion (SLAB/UGC model)
enable_dense_gas = true

# Source location and emission
source_x  = 250.0       # Source x-coordinate [m] (center)
source_y  = 250.0       # Source y-coordinate [m] (center)
source_z  = 0.5         # Source height [m] (near ground, dense gas release)
emission_rate = 1.0     # Emission rate [kg/s]
emission_duration = 100.0  # Duration of emission [s]

# Dense gas species parameters (CO₂)
gas_molecular_weight = 44.01    # CO₂ molar mass [g/mol]
gas_density = 1.98              # CO₂ density at std conditions [kg/m³]
initial_layer_height = 5.0      # Initial SLAB layer height H₀ [m]
slab_decay_scale = 200.0        # Characteristic decay scale x_max [m]
slab_power_exponent = 0.667     # SLAB height decay exponent (2/3)
lateral_spreading_coeff = 0.15  # Lateral spreading coefficient
entrainment_coefficient = 0.1   # Vertical mixing entrainment

# Diffusivity parameters (base values, modified by dense gas effects)
K_h = 0.5               # Horizontal diffusivity [m²/s] (reduced for dense gas)
K_v = 0.3               # Vertical diffusivity [m²/s] (reduced)

# Initial puff size
sigma_y0 = 2.0          # Initial lateral width [m]
sigma_z0 = 2.0          # Initial vertical height [m]

# Wind field (uniform, lower speed for gravity-dominated regime)
U_wind = 5.0            # x-component of wind [m/s] (5 m/s gives Fr ~ 0.5-1.0)
V_wind = 0.0            # y-component of wind [m/s]
W_wind = 0.0            # z-component of wind [m/s]

# Domain extent (flat terrain)
xmin = 0.0              # Domain min x [m]
xmax = 500.0            # Domain max x [m]
ymin = 0.0              # Domain min y [m]
ymax = 500.0            # Domain max y [m]
zmin = 0.0              # Domain min z [m]
zmax = 100.0            # Domain max z [m]

# Concentration grid resolution
dx = 10.0               # Grid spacing x [m]
dy = 10.0               # Grid spacing y [m]
dz = 2.0                # Fine vertical resolution for dense layer [m]

# Time stepping
dt_puff = 0.5           # Time step [s]
n_steps_puff = 200      # Number of time steps (total time = 100 s)
output_freq_puff = 20   # Write concentration every 20 steps (10 s intervals)

# Output file
puff_output = puff_dense_gas_co2.csv  # Output file
