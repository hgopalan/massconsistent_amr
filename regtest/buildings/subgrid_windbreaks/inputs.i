# Sub-grid Windbreaks and Linear Barriers Test
# Tests: Localized directional drag of thin unresolved obstacles.

terrain_file = terrain.csv

U_ref = 10.0
V_ref = 0.0
z_ref = 20.0

z0 = 0.05

# Spacings
dx = 25.0
dy = 25.0
dz = 2.0

domain_height = 100.0

# Enable windbreaks model
enable_windbreaks = true
windbreaks_file = windbreaks.csv

alpha_h = 1.0
alpha_v = 1.0

mlmg_verbose  = 0
max_grid_size = 32

plot_file = plt_subgrid_windbreaks
