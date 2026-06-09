# Precipitation stability adjustments test
terrain_file = terrain.csv
time_series_file = time_series.csv
precipitation_file = precipitation.csv

enable_time_varying = true
init_mode = loglaw

z_ref = 10.0
z0 = 0.1

enable_pg_stability = true
solar_radiation = 500.0
is_nighttime = false
enable_stability_correction = true
precipitation_stability_threshold = 1.0

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
plot_file = plt_precipitation_stability
