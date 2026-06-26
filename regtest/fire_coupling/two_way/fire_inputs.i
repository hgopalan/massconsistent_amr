# Fire solver inputs for two-way coupling test

# Grid configuration
n_cell_x = 32
n_cell_y = 32

# Domain bounds (must match wind solver)
plo_x = 0.0
plo_y = 0.0
phi_x = 1000.0
phi_y = 1000.0

# AMR configuration
max_grid = 32

# Ignition setup
ignition.type = circle
ignition.x0 = 250.0
ignition.y0 = 250.0
ignition.radius = 50.0
ignition.time = 0.0

# Time control
cfl = 0.5
nsteps = 20
max_time = 600.0

# Propagation
propagation_method = levelset

# Fuel model
rothermel.model_number = 1

# Output
plot_interval = 1000000
write_plotfile = 0
