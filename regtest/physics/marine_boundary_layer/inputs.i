# Marine Boundary Layer mixing height test
terrain_file = terrain.csv
landuse_file = landuse.csv

init_mode = uniform
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

enable_landuse_roughness = true
enable_marine_bl = true
marine_sst = 288.15
marine_air_sea_dt = -2.0

# Grid spacing [m]
dx = 50.0
dy = 50.0
dz = 25.0

domain_height = 100.0

alpha_h = 1.0
alpha_v = 1.0

mlmg_verbose  = 0
max_grid_size = 32

extract_agl  = 15.0
extract_file = wind_extract.csv
plot_file = plt_marine_bl
