# Puff Deposition/Sedimentation Test Case 
# Tests: Dry deposition of pollutants to ground surface
# Particles settle due to gravity, gases deposit on surfaces
# Domain: 300m x 300m x 100m
# Source: center of domain at 15m height
# Wind: 5 m/s from west (positive x-direction)

# Enable puff model
enable_puff = true

# Source location and emission
source_x  = 150.0      # Source x-coordinate [m]
source_y  = 150.0      # Source y-coordinate [m]
source_z  = 15.0       # Source height above ground [m]
emission_rate = 1.0    # Emission rate [units/s]
emission_duration = 60.0  # Duration of emission [s]

# Diffusivity parameters
K_h = 1.5              # Horizontal diffusivity [m²/s]
K_v = 0.8              # Vertical diffusivity [m²/s]

# Initial puff size
sigma_y0 = 1.5         # Initial lateral width [m]
sigma_z0 = 1.5         # Initial vertical height [m]

# Enable deposition/sedimentation
enable_puff_deposition = true
deposition_velocity = 0.01  # Dry deposition velocity [m/s]
                            # Typical for particles: 0.001-0.1 m/s
                            # Typical for SO2: 0.005-0.01 m/s

# Wind field (uniform for this test)
U_wind = 5.0           # x-component of wind [m/s]
V_wind = 0.0           # y-component of wind [m/s]
W_wind = -0.2          # z-component (slight downward motion) [m/s]

# Domain extent
xmin = 0.0             # Domain min x [m]
xmax = 300.0           # Domain max x [m]
ymin = 0.0             # Domain min y [m]
ymax = 300.0           # Domain max y [m]
zmin = 0.0             # Domain min z [m]
zmax = 100.0           # Domain max z [m]

# Concentration grid resolution
dx = 10.0              # Grid spacing x [m]
dy = 10.0              # Grid spacing y [m]
dz = 10.0              # Grid spacing z [m]

# Time stepping
dt_puff = 1.0          # Time step [s]
n_steps_puff = 120     # Number of time steps (total time = 120 s)
output_freq_puff = 10  # Write concentration every 10 steps

# Output file
puff_output = puff_deposition.csv  # Output file prefix
