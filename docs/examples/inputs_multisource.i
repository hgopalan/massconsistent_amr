# Example: Multi-source puff model with stack downwash
# This demonstrates Phase 2 features:
# - Multiple simultaneous sources from sources_multisource.csv
# - Stack aerodynamic modeling with Briggs downwash
# - Multiple meteorological profiles

enable_puff = true
enable_lpdm = false

# Multi-source configuration (Phase 2.1)
sources_file = "docs/examples/sources_multisource.csv"

# Stack aerodynamic modeling (Phase 2.2)
stack_tip_downwash_enabled = true
briggs_std_model = true

# Meteorological profiles (Phase 2.3)
met_profile_file = "docs/examples/met_profiles_spatial.csv"
enable_spatial_met = true

# Diffusivity and initial puff size
K_h = 1.0
K_v = 0.5
sigma_y0 = 1.0
sigma_z0 = 1.0

# Time stepping
dt_puff = 1.0
n_steps_puff = 100
output_freq_puff = 10

# Wind field
U_wind = 10.0
V_wind = 0.0
W_wind = 0.0

# Domain
xmin = 0.0
xmax = 300.0
ymin = 0.0
ymax = 300.0
zmin = 0.0
zmax = 100.0
dx = 10.0
dy = 10.0
dz = 10.0

# Output
puff_output = "puff_multisource.csv"

# Plume rise for buoyant sources
enable_plume_rise = false
heat_flux = 0.0

# Pasquill-Gifford stability class
pg_stability_class = 3  # Neutral (D)
