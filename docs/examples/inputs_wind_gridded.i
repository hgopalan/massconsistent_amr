# Example: Puff Model with Gridded Wind from CSV
# This example demonstrates how to use gridded wind fields

enable_puff = true

# Source configuration
source_x  = 150.0
source_y  = 150.0
source_z  = 10.0
emission_rate = 1.0
emission_duration = 50.0

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
dt_puff = 0.5
n_steps_puff = 100
output_freq_puff = 10

# Output file
puff_output = puff_concentration.csv

# Wind field input configuration for gridded wind
wind_field_file = "wind_field_gridded.csv"
wind_field_format = "gridded"
