# Fire solver configuration - Colorado one-way coupling scenario
# One-way coupling: Wind → Fire (fire does not affect wind)
# Domain: 10 km x 10 km (matches wind solver domain)
# Grid: 156 x 156 cells (dx=dy=64m)

# Grid configuration
n_cell_x = 156
n_cell_y = 156

# Domain bounds (must match wind solver: 0-10km in x,y)
prob_lo_x = 0.0
prob_lo_y = 0.0
prob_hi_x = 10000.0
prob_hi_y = 10000.0

# AMR configuration
max_grid = 32
blocking_factor = 16

# Ignition setup - circular fire at center of domain
# Center: (5000, 5000) = domain center
# Radius: 256 m
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
rothermel.model_number = 1  # Short grass
rothermel.fuel_moisture = 0.15

# Wind interaction
use_wind_field = 1
wind_speed_factor = 1.0

# Output
plot_interval = 10
write_plotfile = 1
plot_fields = phi,ros,intensity

# Verbose output
verbose = 1
