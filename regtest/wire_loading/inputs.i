# Flat Terrain Wind Solver Test with Electrical Wires
terrain_file = terrain.csv

# Enable Electrical Wire Loading
enable_wire_loading = true
wire_file = wires.csv
wire_output_file = wire_output.csv

# Reference wind: 5 m/s from west at 10 m AGL
U_ref = 5.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
z0 = 0.1

# Grid spacing [m]
dx = 10.0
dy = 10.0
dz = 10.0

# Domain height [m] above maximum terrain elevation
domain_height = 100.0

# Lagrange anisotropy coefficients
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver settings (silent, default tolerances)
mlmg_verbose  = 0
max_grid_size = 32

# Output plotfile
plot_file = plt_wire_loading
