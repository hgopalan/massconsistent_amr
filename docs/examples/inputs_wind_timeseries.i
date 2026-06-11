# Example: Puff Model with Time-Varying Wind from CSV
# This example demonstrates how to use time-series wind fields

enable_puff = true

# Source configuration
source_x  = 150.0
source_y  = 150.0
source_z  = 10.0
emission_rate = 1.0
emission_duration = 240.0

# Diffusivity parameters
K_h = 1.0
K_v = 0.5
sigma_y0 = 1.0
sigma_z0 = 1.0

# Domain extent
xmin = 0.0
xmax = 300.0
ymin = 0.0
ymax = 300.0
zmin = 0.0
zmax = 100.0

# Concentration grid resolution
dx = 10.0
dy = 10.0
dz = 10.0

# Time stepping
dt_puff = 1.0
n_steps_puff = 300
output_freq_puff = 10

# Output file
puff_output = puff_concentration.csv

# Wind field input configuration for time-varying wind
wind_field_file = "wind_field_timeseries.csv"
wind_field_format = "timeseries"
enable_unsteady_wind = true
wind_field_start_time = 0.0
