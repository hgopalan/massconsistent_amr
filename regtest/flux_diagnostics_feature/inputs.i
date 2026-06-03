# Phase 1: Flux Diagnostics Feature Test
# Tests: Sensible heat flux (SHF), latent heat flux (LHF), momentum flux, drag coefficient
# This test verifies that diagnostic flux fields are computed and output correctly
# Terrain: 3x3 flat domain, simple geometry to isolate flux calculations

# Terrain file
terrain_file = terrain.csv

# Log-law initialization with unstable conditions
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Roughness for flux calculations
z0 = 0.1

# Grid parameters
dx = 50.0
dy = 50.0
dz = 25.0
domain_height = 100.0

# Mass-consistent parameters
alpha_h = 1.0
alpha_v = 1.0

# MLMG solver (silent mode)
mlmg_verbose  = 0
max_grid_size = 32

# Phase 1 flux diagnostic parameters
# Enable surface flux diagnostics (SHF, LHF, momentum flux, drag coefficient)
enable_flux_diagnostics = true

# Temperature scaling for heat flux calculation (affects both SHF and LHF)
# These would typically be from surface models or atmospheric observations
surface_temperature = 300.0  # Reference temperature [K]
heat_flux_scale = 1.0       # Scaling factor for sensible heat flux

# Moisture for latent heat flux
relative_humidity = 0.5     # Relative humidity for evaporation calculation

# Extract wind at 10 m AGL to verify surface layer
extract_agl  = 10.0
extract_file = wind_extract.csv

# Output plotfile with flux diagnostics
plot_file = plt_flux_diagnostics_feature
