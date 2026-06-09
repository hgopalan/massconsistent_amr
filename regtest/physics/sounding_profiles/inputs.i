# Sounding profiles interpolation test
terrain_file = terrain.csv

init_mode = sounding
sounding_files = sounding1.fsl sounding2.up
sounding_x = 100.0 900.0
sounding_y = 100.0 900.0
sounding_vertical_interp = spline
sounding_wind_in_knots = false

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 50.0

domain_height = 500.0

alpha_h = 1.0
alpha_v = 1.0

mlmg_verbose  = 0
max_grid_size = 32

extract_agl  = 50.0 200.0
extract_file = wind_extract.csv
plot_file = plt_sounding_profiles
