# Gaussian Puff Model - Building Wake Test
# Tests building masking and wake-enhanced diffusivity
# Domain: 300m x 300m x 100m  
# Source: upwind of building at 10m height
# Building: 50m x 40m x 30m tall at domain center
# Wind: 10 m/s from west

# Enable puff model
enable_puff = true

# Source location and emission (upwind of building)
source_x  = 80.0       # Source x-coordinate [m] (upwind)
source_y  = 150.0      # Source y-coordinate [m] (centerline)
source_z  = 10.0       # Source height [m]
emission_rate = 1.0    # Emission rate [units/s]
emission_duration = 50.0  # Duration of emission [s]

# Diffusivity parameters
K_h = 1.0              # Horizontal diffusivity [m²/s]
K_v = 0.5              # Vertical diffusivity [m²/s]

# Initial puff size
sigma_y0 = 1.0         # Initial lateral width [m]
sigma_z0 = 1.0         # Initial vertical height [m]

# Wind field (uniform for this test)
U_wind = 10.0          # x-component of wind [m/s]
V_wind = 0.0           # y-component of wind [m/s]
W_wind = 0.0           # z-component of wind [m/s]

# Building awareness
building_file = buildings.csv        # Single building
enable_building_masking = true       # Enable collision detection
enable_wake_diffusivity = true       # Enable wake enhancement

# Wake model parameters (Röckle 1990)
wake_c1 = 0.9                        # Cavity length coefficient
wake_c2 = 0.3                        # Wake deficit coefficient
wake_separation_length = 3.0         # Wake extends to 3*H downwind

# Wake diffusivity enhancement factors
wake_enhancement_cavity = 3.0        # 3x enhancement in cavity
wake_enhancement_far = 1.5           # 1.5x enhancement in far wake

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
output_freq_puff = 20  # Write concentration every 20 steps

# Output file
puff_output = puff_concentration_buildings.csv  # Output file prefix
