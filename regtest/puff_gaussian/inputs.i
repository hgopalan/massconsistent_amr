# Gaussian Puff Model Test Case
# Simple uniform wind field with point source dispersion
# Domain: 300m x 300m x 100m
# Source: center of domain at 10m height
# Wind: 10 m/s from west (positive x-direction)

# Enable puff model
enable_puff = true

# Source location and emission
source_x  = 150.0      # Source x-coordinate [m]
source_y  = 150.0      # Source y-coordinate [m]
source_z  = 10.0       # Source height above ground [m]
emission_rate = 1.0    # Emission rate [units/s]
emission_duration = 50.0  # Duration of emission [s]

# Diffusivity parameters (Smagorinsky-like turbulent diffusion)
K_h = 1.0              # Horizontal diffusivity [m²/s]
K_v = 0.5              # Vertical diffusivity [m²/s]

# Initial puff size
sigma_y0 = 1.0         # Initial lateral width [m]
sigma_z0 = 1.0         # Initial vertical height [m]

# Wind field (uniform for this test)
U_wind = 10.0          # x-component of wind [m/s]
V_wind = 0.0           # y-component of wind [m/s]
W_wind = 0.0           # z-component of wind [m/s]

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
dt_puff = 0.5          # Time step [s]
n_steps_puff = 100     # Number of time steps (total time = 50 s)
output_freq_puff = 10  # Write concentration every 10 steps

# Output file
puff_output = puff_concentration.csv  # Output file prefix
