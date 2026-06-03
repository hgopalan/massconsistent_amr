# Multi-Scale Terrain Analysis Test
# Tests: terrain classification and adaptive parameterization
# Verifies: terrain_type field matches slope-based classification
# Expected: flat=0, moderate=1, steep=2 regions based on thresholds

terrain_file = terrain.csv

U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.03

dx = 30.0
dy = 30.0
dz = 25.0

domain_height = 100.0

alpha_h = 1.0
alpha_v = 1.0

# Feature 22: Multi-Scale Terrain Analysis
enable_terrain_analysis = true
slope_threshold_moderate = 0.15
slope_threshold_steep = 0.35
roughness_factor_moderate = 0.25
roughness_factor_steep = 0.8
transition_zone_width = 0.03

mlmg_verbose = 0
max_grid_size = 32

extract_agl = 15.0
extract_file = wind_extract.csv

plot_file = plt_terrain_analysis
