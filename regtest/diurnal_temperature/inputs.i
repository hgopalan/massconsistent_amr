# Feature 4: Diurnal Temperature Profile Test
# Tests: Time-varying sinusoidal temperature variation
# Simulates daily solar heating cycle

# Terrain file (flat domain)
terrain_file = terrain.csv

# Log-law initialization
init_mode = loglaw
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

# Grid spacing [m]
dx = 30.0
dy = 30.0
dz = 30.0

# Domain height [m] above maximum terrain elevation
domain_height = 300.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# Enable buoyancy stratification with temperature profile
enable_buoyancy_stratification = true
temperature_file = temperature.csv
temperature_reference = 298.0  # Reference temperature [K]
buoyancy_coefficient = 1.0
buoyancy_timescale = 10.0
buoyancy_method = velocity

# Feature 4: Enable diurnal temperature variation
enable_diurnal_temperature = true
diurnal_temperature_amplitude = 5.0  # +/- 5K variation
diurnal_time_of_day = 14.0           # Current time: 14:00 (peak heating)
diurnal_phase_hour = 14.0            # Maximum temperature at 14:00
diurnal_period = 24.0                # 24-hour cycle

# MLMG solver settings (silent)
mlmg_verbose  = 0
max_grid_size = 32

# Extract wind at 15 m AGL
extract_agl  = 15.0
extract_file = wind_extract.csv

# Output plotfile
plot_file = plt_diurnal_temperature
