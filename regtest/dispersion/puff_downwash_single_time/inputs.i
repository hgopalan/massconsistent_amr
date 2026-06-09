# Gaussian Puff Model - Advanced Building Downwash Test (Single Time Instance)
# Tests cavity trapping parameterization (AERMOD PRIME)
# Domain: 400m x 300m x 100m
# Source: upwind of single building at 15m height
# Building: 50m x 40m x 30m tall at domain center
# Wind: 8 m/s from west (positive x-direction)
# Simulation: Single time step for cavity zone verification

# Enable puff model
enable_puff = true

# Advanced downwash features
enable_cavity_trapping = true
enable_plume_deformation = false

# Source location and emission
source_x  = 100.0      # Source x-coordinate [m] (upwind of building)
source_y  = 150.0      # Source y-coordinate [m] (centerline)
source_z  = 15.0       # Source height [m]
emission_rate = 2.0    # Emission rate [units/s]
emission_duration = 0.5  # Duration of emission [s] (single puff)

# Diffusivity parameters
K_h = 1.0              # Horizontal diffusivity [m²/s]
K_v = 0.5              # Vertical diffusivity [m²/s]

# Initial puff size
sigma_y0 = 2.0         # Initial lateral width [m]
sigma_z0 = 2.0         # Initial vertical height [m]

# Wind field (uniform for this test)
U_wind = 8.0           # x-component of wind [m/s]
V_wind = 0.0           # y-component of wind [m/s]
W_wind = 0.0           # z-component of wind [m/s]

# Building awareness
building_file = buildings.csv
enable_building_masking = true
enable_wake_diffusivity = true

# Wake model parameters
wake_c1 = 0.9                        # Cavity length coefficient
wake_c2 = 0.3                        # Wake deficit coefficient
wake_enhancement_cavity = 3.0        # 3x enhancement in cavity
wake_enhancement_far = 1.5           # 1.5x enhancement in far wake

# Cavity trapping parameters (AERMOD PRIME)
aermod_prime_cavity_factor = 0.67    # Cavity height factor (0.67*H)
cavity_recirculation_strength = 0.8  # Recirculation intensity [0-1]

# Domain extent
xmin = 0.0             # Domain min x [m]
xmax = 400.0           # Domain max x [m]
ymin = 0.0             # Domain min y [m]
ymax = 300.0           # Domain max y [m]
zmin = 0.0             # Domain min z [m]
zmax = 100.0           # Domain max z [m]

# Concentration grid resolution
dx = 10.0              # Grid spacing x [m]
dy = 10.0              # Grid spacing y [m]
dz = 5.0               # Grid spacing z [m]

# Time stepping
dt_puff = 1.0          # Time step [s]
n_steps_puff = 1       # Number of time steps (single step test)
output_freq_puff = 1   # Write concentration every step

# Output file
puff_output = puff_downwash_single
