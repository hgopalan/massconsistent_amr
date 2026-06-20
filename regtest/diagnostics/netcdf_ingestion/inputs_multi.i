# NetCDF multiple time instances wind solver test
terrain_file = terrain.csv

init_mode = windfield
windfield_file = windfield_multi.csv

dx = 30.0
dy = 30.0
dz = 25.0

domain_height = 100.0

alpha_h = 1.0
alpha_v = 1.0

mlmg_verbose  = 0
max_grid_size = 32

extract_agl  = 15.0
extract_file = wind_extract_multi.csv

plot_file = plt_netcdf_multi

num_time_steps = 1
