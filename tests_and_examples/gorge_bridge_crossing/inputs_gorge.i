
# Gorge Bridge Crossing - Complex Terrain Wind Effects
# Deep canyon with asymmetric walls, vortex formation downstream
# Expected sway: 0.5-1.5 m, comfort assessment potentially unsafe

terrain_file = gorge_terrain.csv

# Enable bridge loading
enable_bridge_loading = true
bridge_file = gorge_bridge.csv
bridge_output_file = gorge_bridge_output.csv

# Reference wind: 10 m/s from valley-parallel direction
# Canyon alignment amplifies to 15-20 m/s in narrowest section
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Roughness: bare rock canyon walls
z0 = 0.05

# Valley/canyon channeling model
enable_valley_channeling = true
enable_gap_flow = false

# Orographic speedup for terrain-aligned wind
enable_orographic_speedup = true

# Grid spacing [m]
# Fine resolution to resolve canyon geometry
dx = 100.0
dy = 100.0
dz = 50.0

# Domain extent
domain_height = 600.0

# Anisotropy: strong horizontal preferencing (valley alignment)
alpha_h = 1.5
alpha_v = 0.7
enable_cell_local_anisotropy = true

# Stability: assume neutral (typical high wind conditions)
enable_stability_correction = false

# MLMG solver
mlmg_verbose = 1
max_grid_size = 32

# Output
plot_file = plt_gorge
num_time_steps = 1
