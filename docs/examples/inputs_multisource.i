# Example: Phase 4 CSV-driven puff model configuration
# Demonstrates:
# - Multi-source emissions from CSV
# - Time-varying emission rates
# - Spatial meteorology profiles
# - Deposition and receptor templates

enable_puff = true
enable_lpdm = false

# Phase 4 CSV Input Files (all optional)
puff_model.sources_file = "docs/examples/sources_multisource.csv"
puff_model.emissions_timeseries_file = "docs/examples/emissions_time_series.csv"
puff_model.deposition_params_file = "docs/examples/deposition_params.csv"
puff_model.met_profiles_file = "docs/examples/met_profiles_spatial.csv"
puff_model.receptors_file = "docs/examples/receptors_grid.csv"

# Stack aerodynamic modeling (Phase 2.2)
stack_tip_downwash_enabled = true
briggs_std_model = true

# Meteorological profiles (Phase 2.3)
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
zmax = 120.0
dx = 10.0
dy = 10.0
dz = 10.0

# Output
puff_output = "puff_multisource.csv"
receptor_output = "puff_receptors.csv"

# Plume rise for buoyant sources
enable_plume_rise = false
heat_flux = 0.0

# Pasquill-Gifford stability class
pg_stability_class = 3  # Neutral (D)
