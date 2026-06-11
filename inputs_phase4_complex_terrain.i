# Annotated: Complex Terrain with Deposition
# ===========================================
#
# Complex terrain (e.g., valleys, ridges) with particle deposition.
# Demonstrates terrain-aware turbulence, plume modification, and settling.
#
# Use case: Mountain valleys, industrial sites in valleys, regional dust

xmin = -5000.0
xmax = 20000.0
ymin = -5000.0
ymax = 20000.0
zmin = 0.0
zmax = 2000.0

n_cell_x = 250
n_cell_y = 250
n_cell_z = 40

dx = 100.0
dy = 100.0

# === TERRAIN ===
terrain_file = terrain_complex.csv

# === WIND WITH TERRAIN INTERACTION ===
init_mode = loglaw
U_ref = 12.0
V_ref = 0.0
z_ref = 100.0
z0 = 1.0  # Rough surface (mountains)

# === PUFF MODEL ===
puff_model.enabled = true

# === CSV INPUT FILES (Phase 4.1) ===
puff_model.sources_file = sources_mountain.csv
puff_model.met_profiles_file = met_profiles_mountain.csv
puff_model.receptors_file = receptors_mountain_grid.csv
puff_model.deposition_params_file = deposition_dust.csv

# === MULTI-SOURCE (Phase 4.1) ===
# File sources_mountain.csv contains:
#   Multiple mining/industrial sites at different elevations
#   Each with unique stack parameters and dust composition

# === PLUME RISE (Phase 2) ===
puff_model.enable_plume_rise = true

# === TERRAIN EFFECTS (Phase 3) ===
# Terrain-aware synthetic turbulence (Phase 4)
puff_model.enable_terrain_aware_turbulence = true
puff_model.terrain_mask_radius = 500.0
puff_model.boundary_blend_factor = 0.5

# Terrain reflection for particles
puff_model.enable_terrain_reflection = true
puff_model.use_image_source = true
puff_model.terrain_reflection_coefficient = 0.8

# === WIND SHEAR & VEERING ===
puff_model.enable_wind_shear = true
puff_model.wind_shear_coefficient = 0.08
puff_model.veer_angle = 20.0

# === DEPOSITION (Phase 3) ===
# Particle settling and dry deposition
puff_model.enable_settling = true
puff_model.enable_puff_deposition = true

# Multiple particle size classes
puff_model.n_particle_classes = 8

# Wet deposition if rain occurs
puff_model.enable_wet_deposition = true
puff_model.rain_rate = 1.0  # mm/hr (light rain)

# === CHEMISTRY (Optional - disable for dust only) ===
puff_model.enable_reactive_chemistry = false

# === DIFFUSIVITY ===
puff_model.K_h = 30.0  # Higher due to terrain-induced mixing
puff_model.K_v = 8.0
puff_model.enable_height_dependent_K = true
puff_model.K_profile = stability

# === VISIBILITY ===
puff_model.enable_optical_properties = true
puff_model.compute_visibility_at_receptors = true

# === OUTPUT ===
puff_model.receptor_output_file = mountain_dust_results.csv
puff_model.grid_output_frequency = 0
puff_model.output_frequency = 600.0  # Every 10 minutes (longer simulation)

# === SIMULATION ===
# Simulate 12-hour period (dust transport over day)
time_stop = 43200.0
dt_base = 1.0
puff_model.dt_puff = 20.0  # Larger dt for stability with larger domain
puff_model.n_steps_puff = 2160  # 43200 / 20

# === SETUP WORKFLOW ===
#
# 1. Prepare terrain file (terrain_complex.csv):
#    - If you have DEM data: convert using external tool (gdal, etc.)
#    - Or use analytical terrain: Gaussian hill, valley model
#    - Format: x [m], y [m], z_terrain [m]
#
# 2. Create receptor grid (valley + ridge):
#    python receptor_grid_generator.py --grid 2d \
#      --nx 30 --ny 30 --x0 2500 --y0 2500 --output receptors_mountain_grid.csv
#
# 3. Prepare multi-source file (sources_mountain.csv):
#    Source 1: Mine at lower elevation (y=3000, z=500)
#    Source 2: Processing plant at mid elevation (y=5000, z=700)
#    Source 3: Storage area at ridge (y=8000, z=1200)
#
# 4. Define meteorology profiles (met_profiles_mountain.csv):
#    - Valley profile (stable, lower wind)
#    - Ridge profile (unstable, higher wind)
#    - Transition profiles by height
#
# 5. Configure deposition parameters (deposition_dust.csv):
#    - 8 dust size classes (1-100 μm)
#    - Settling velocities for each class
#    - Dry deposition velocities (terrain-dependent)
#    - Wet scavenging coefficients
#
# 6. Run simulation:
#    ./wind_solver inputs_phase4_complex_terrain.i
#
# 7. Analysis:
#    - Plot deposition field (lateral extent, peak flux)
#    - Compare concentration at valley bottom vs. ridge
#    - Validate with measurements if available

# === EXPECTED BEHAVIOR ===
#
# Valley Flow:
# - Wind channeled along valley
# - Reduced mixing → higher concentrations
# - Terrain slope effects on plume rise
#
# Deposition Patterns:
# - Heavy dust settles near source
# - Fine particles transport 5-20 km
# - Ridge barriers can redirect plume
#
# Visibility:
# - Worst visibility in valley (highest concentration)
# - Improved visibility on ridges (lower concentration)
# - Night-time inversion traps dust → very poor visibility

# === VISUALIZATION (Post-processing) ===
#
# Generate deposition map:
#   - Extract receptor deposition fluxes
#   - Gridding and contouring
#   - Overlay on terrain model
#
# Generate concentration profiles:
#   - Cross-section perpendicular to wind
#   - Height profile above valley floor
#   - Time series at fixed locations

# === TROUBLESHOOTING ===
#
# If concentrations too high:
# - Increase K_h/K_v
# - Check terrain file for errors (negative values, gaps)
# - Verify emission rates reasonable
#
# If deposition unrealistic:
# - Check particle size distribution
# - Verify settling velocities in deposition_dust.csv
# - Check rain rate if wet deposition enabled
#
# If terrain reflection causes oscillations:
# - Reduce reflection coefficient
# - Increase boundary blend factor
# - Disable image source method

# === REFERENCES ===
# - Barrier effects on plumes: Lyons & Scott (1990)
# - Valley wind systems: Serafin et al. (2018)
# - Dust deposition: Marticorena & Bergametti (1995)
# - CALPUFF terrain processing: Scire et al. (2000)
# - Particle settling: Saffman (1965)
