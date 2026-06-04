# Canopy Vertical LAI Profiles Test
# Tests: Coniferous profile (canopy_profile_type = 1) with mid-canopy peak density
# This test validates the vertically-varying foliage density distribution.

terrain_file = terrain.csv

U_ref = 10.0
V_ref = 0.0
z_ref = 40.0

z0 = 0.05

dx = 50.0
dy = 50.0
dz = 2.5

domain_height = 150.0

# Canopy model parameters
enable_canopy = true
canopy_height = 15.0              # Canopy height [m]
frontal_area_index = 0.30         # Dense canopy
plan_area_index = 0.25
canopy_drag_coeff = 0.25

# Enable Shaw-Pereira exponential decay with coniferous profile
use_exponential_profile = true
canopy_attenuation = 2.5          # Attenuation coefficient (α)
canopy_profile_type = 1           # 1 = Coniferous

alpha_h = 1.0
alpha_v = 1.0

mlmg_verbose  = 0
max_grid_size = 32

extract_agl  = 7.5
extract_file = wind_extract_7.5m.csv

plot_file = plt_canopy_lai_profiles
