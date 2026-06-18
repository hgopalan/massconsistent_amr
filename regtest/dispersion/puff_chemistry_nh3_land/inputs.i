# Ammonia Oxidation Chemistry Over Land Regression Test
# Validates NH₃ oxidation with first-order decay: C(t) = C₀ × exp(-t/τ)
# Expected: ~50% decay at t = 4.5 days for 9-day half-life
#
# Physical scenario:
# - Agricultural/industrial ammonia emission (bunkering, storage, or livestock)
# - Point source at 10 m height (typical stack/vent)
# - Over land with vegetated/rural terrain
# - Uniform wind field at 5 m/s (moderate)

# Enable puff model
enable_puff = true

# Source location and emission
source_x  = 150.0      # Source x-coordinate [m]
source_y  = 150.0      # Source y-coordinate [m]
source_z  = 10.0       # Source height [m] (stack/vent height)
emission_rate = 1.0    # Emission rate [kg/s]
emission_duration = 1.0  # Single puff release

# Diffusivity parameters (land scenario)
K_h = 1.0              # Horizontal diffusivity [m²/s]
K_v = 0.5              # Vertical diffusivity [m²/s]

# Initial puff size
sigma_y0 = 1.0         # Initial lateral width [m]
sigma_z0 = 1.0         # Initial vertical height [m]

# Wind field (moderate, rural)
U_wind = 5.0           # x-component of wind [m/s]
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

# Time stepping: ~4.5 days (388800 seconds) to observe ~50% decay
dt_puff = 100.0        # Time step [s]
n_steps_puff = 3888    # Total time = 388,800 s = 4.5 days
output_freq_puff = 96  # Write every 9600 s = 2.667 hours

# Output file
puff_output = puff_nh3_land.csv

# Ammonia chemistry parameters
enable_ammonia_chemistry = true
ammonia_half_life_land = 216.0      # 9-day half-life over land [hours]
ammonia_enable_water_exchange = false  # Land scenario (no water exchange)
ammonia_enable_seasonal_adjust = false  # No seasonal variation for basic test
ammonia_enable_temp_adjust = false     # Fixed temperature
