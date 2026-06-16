# NO₂ Photochemical Decay Regression Test
# Simple uniform wind field with NO₂ source
# Validates 1st-order exponential decay: C(t) = C₀ × exp(-t/τ)
# Expected: ~50% decay at t = 4 hours for 100 ppb initial

# Enable puff model
enable_puff = true

# Source location and emission
source_x  = 150.0      # Source x-coordinate [m]
source_y  = 150.0      # Source y-coordinate [m]
source_z  = 10.0       # Source height above ground [m]
emission_rate = 1.0    # Emission rate [kg/s]
emission_duration = 1.0  # Short duration (single puff)

# Diffusivity parameters
K_h = 1.0              # Horizontal diffusivity [m²/s]
K_v = 0.5              # Vertical diffusivity [m²/s]

# Initial puff size
sigma_y0 = 1.0         # Initial lateral width [m]
sigma_z0 = 1.0         # Initial vertical height [m]

# Wind field (uniform)
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

# Time stepping: 14400 seconds = 4 hours
# At 10 m/s wind, puff travels 144 km in 4 hours
dt_puff = 100.0        # Time step [s]
n_steps_puff = 144     # Total time = 14400 s = 4 hours
output_freq_puff = 1   # Write every step

# Output file
puff_output = puff_concentration.csv  # Output file prefix

# Chemical species parameters
puff_chemistry_enabled = true
puff_chemistry_half_life_NO2 = 4.0   # 4-hour half-life
puff_chemistry_enable_products = true  # Track NO production
puff_chemistry_enable_seasonal_adjust = false  # No seasonal variation
puff_chemistry_enable_temp_adjust = false      # No temperature variation

# Initial chemical concentrations [ppb]
puff_initial_NO2 = 100.0     # 100 ppb NO₂
puff_initial_NO = 0.0        # No initial NO
