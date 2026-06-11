# Terrain-masked synthetic turbulence over Gaussian hill
# Domain setup with terrain.csv
terrain_file = terrain.csv
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.05

dx = 25.0
dy = 25.0
dz = 10.0
domain_height = 140.0
plot_file = plt_terrain_masked_synthesis

# Requested terrain-aware turbulence switches
enable_synthetic_turbulence = true
turbulence_model = kaimal
terrain_aware_masking = true
enable_boundary_blending = true
export_turbulence_bts = true
turbulence_bts_file = synthetic_turbulence.bts

# Compatible legacy controls used by existing turbulence infrastructure
turbulence_spectrum_model = Kaimal
turbulence_export_format = bts
turbulence_output_file = synthetic_turbulence.bts
turbulence_intensity_ref = 0.14
turbulence_length_scale_u = 220.0
turbulence_length_scale_v = 150.0
turbulence_length_scale_w = 90.0
turbulence_random_seed = 2026
