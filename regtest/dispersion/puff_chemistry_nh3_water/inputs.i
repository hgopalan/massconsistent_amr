# Ammonia Gas-Liquid Exchange Over Water Regression Test
# Validates NH₃ absorption at air-water interface using simplified two-film theory
# Expected: Rapid removal of ~50-80% in first few hours due to high solubility
#
# Physical scenario:
# - Ammonia bunkering operation over water (ship fuel transfer)
# - Or ammonia storage facility near coastal/lake water
# - Point source at 5 m height (container/manifold level)
# - Moderate wind (5 m/s) with 15°C fresh water

# Enable puff model
enable_puff = true

# Source location and emission
source_x  = 250.0      # Source x-coordinate [m]
source_y  = 250.0      # Source y-coordinate [m]
source_z  = 5.0        # Source height [m] (container/manifold level, lower over water)
emission_rate = 1.0    # Emission rate [kg/s]
emission_duration = 1.0  # Single puff release

# Diffusivity parameters (over water - smoother, less turbulence)
K_h = 0.5              # Horizontal diffusivity [m²/s] (reduced over smooth water)
K_v = 0.3              # Vertical diffusivity [m²/s] (reduced over water)

# Initial puff size
sigma_y0 = 1.0         # Initial lateral width [m]
sigma_z0 = 1.0         # Initial vertical height [m]

# Wind field (moderate, over water)
U_wind = 5.0           # x-component of wind [m/s]
V_wind = 0.0           # y-component of wind [m/s]
W_wind = 0.0           # z-component of wind [m/s]

# Domain extent (larger for water to capture far-field)
xmin = 0.0             # Domain min x [m]
xmax = 500.0           # Domain max x [m]
ymin = 0.0             # Domain min y [m]
ymax = 500.0           # Domain max y [m]
zmin = 0.0             # Domain min z [m]
zmax = 100.0           # Domain max z [m]

# Concentration grid resolution
dx = 10.0              # Grid spacing x [m]
dy = 10.0              # Grid spacing y [m]
dz = 10.0              # Grid spacing z [m]

# Time stepping: ~6 hours to observe rapid removal from air via water absorption
dt_puff = 10.0         # Smaller time step [s] for rapid process
n_steps_puff = 2160    # Total time = 21,600 s = 6 hours
output_freq_puff = 36  # Write every 360 s = 6 minutes

# Output file
puff_output = puff_nh3_water.csv

# Ammonia chemistry parameters
enable_ammonia_chemistry = true
ammonia_half_life_land = 216.0      # Not used (over water)
ammonia_enable_water_exchange = true # Water exchange (fast dissolution)
ammonia_water_temperature = 288.15   # Fresh water at 15°C [K]
ammonia_water_salinity = 0.0         # Fresh water [PSU]
ammonia_enable_seasonal_adjust = false  # No seasonal variation
ammonia_enable_temp_adjust = false     # Fixed temperature
