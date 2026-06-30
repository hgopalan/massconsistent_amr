# Fire solver configuration - California two-way coupling scenario
# Two-way coupling: Wind ↔ Fire (fire heating affects wind)
# Domain: 10 km x 10 km

n_cell_x = 156
n_cell_y = 156

# Domain bounds
prob_lo_x = 0.0
prob_lo_y = 0.0
prob_hi_x = 10000.0
prob_hi_y = 10000.0

# AMR configuration
max_grid = 32
blocking_factor = 16

# Ignition setup - circular fire at center
source_type = sphere
center_x = 5000.0
center_y = 5000.0
sphere_radius = 256.0

# Time control
cfl = 0.5
nsteps = 100
max_time = 1200.0

# Propagation method
propagation_method = farsite

# Fuel model (Rothermel fire spread model)
rothermel.model_number = 1
rothermel.fuel_moisture = 0.15

# Wind interaction
use_wind_field = 1
wind_speed_factor = 1.0

# Heat source extraction (for two-way coupling)
extract_heat_source = 1
heat_source_height = 5.0

# Output
plot_interval = 10
write_plotfile = 1
plot_fields = phi,ros,intensity,heat_source

# Verbose output
verbose = 1
