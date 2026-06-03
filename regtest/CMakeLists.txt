# CMakeLists.txt for regression tests
# Tests are organised into sub-folders:
#   flat_terrain/  – flat domain, verifies mass consistency on trivial geometry
#   gaussian_hill/ – Gaussian hill terrain, verifies terrain-following wind field

cmake_minimum_required(VERSION 3.20)
enable_testing()

set(MASSCONSISTENT_EXE "${CMAKE_BINARY_DIR}/wind_solver"
    CACHE FILEPATH "Path to wind_solver executable")

# ---------------------------------------------------------------------------
# Helper: add a solver regression test
# ---------------------------------------------------------------------------
function(add_regression_test TEST_NAME TEST_DIR)
    set(TEST_INPUT    "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/inputs.i")
    set(TEST_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TEST_DIR}")
    file(MAKE_DIRECTORY ${TEST_WORK_DIR})

    # Copy terrain/data CSV files to the working directory
    file(GLOB _DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*.csv")
    foreach(_df ${_DATA_FILES})
        file(COPY "${_df}" DESTINATION ${TEST_WORK_DIR})
    endforeach()

    add_test(NAME ${TEST_NAME}
             COMMAND ${MASSCONSISTENT_EXE} ${TEST_INPUT}
             WORKING_DIRECTORY ${TEST_WORK_DIR})
    set_tests_properties(${TEST_NAME} PROPERTIES
                         TIMEOUT 300
                         LABELS  "regtest")
endfunction()

# ===========================================================================
# flat_terrain/ – flat domain (all z = 0), trivial mass-consistency check
# ===========================================================================
add_regression_test(flat_terrain flat_terrain)

# ===========================================================================
# gaussian_hill/ – Gaussian hill terrain, terrain-following log-law wind
# ===========================================================================
add_regression_test(gaussian_hill gaussian_hill)

# ===========================================================================
# gaussian_hill_weno5/ – Gaussian hill with WENO-5 derivatives
# ===========================================================================
add_regression_test(gaussian_hill_weno5 gaussian_hill_weno5)

# ===========================================================================
# raws_synthetic/ – Gaussian hill with RAWS wind initialization (IDW interp)
# ===========================================================================
add_regression_test(raws_synthetic raws_synthetic)

# ===========================================================================
# surface_data_synthetic/ – Gaussian hill with surface_data initialization
# ===========================================================================
add_regression_test(surface_data_synthetic surface_data_synthetic)

# ===========================================================================
# uniform_wind/ – Gaussian hill with uniform wind initialization
# ===========================================================================
add_regression_test(uniform_wind uniform_wind)

# ===========================================================================
# powerlaw_flat/ – Power-law wind profile on flat terrain
# ===========================================================================
add_regression_test(powerlaw_flat powerlaw_flat)

# ===========================================================================
# powerlaw_gaussian/ – Power-law wind profile on Gaussian hill
# ===========================================================================
add_regression_test(powerlaw_gaussian powerlaw_gaussian)

# ===========================================================================
# powerlaw_building_flat/ – Power-law profile with buildings on flat terrain
# ===========================================================================
add_regression_test(powerlaw_building_flat powerlaw_building_flat)

# ===========================================================================
# powerlaw_building_terrain/ – Power-law profile with buildings on terrain
# ===========================================================================
add_regression_test(powerlaw_building_terrain powerlaw_building_terrain)

# ===========================================================================
# buildings_flat/ – Buildings (union of boxes) on flat terrain
# ===========================================================================
add_regression_test(buildings_flat buildings_flat)

# ===========================================================================
# buildings_csv/ – Buildings from CSV file on flat terrain
# ===========================================================================
add_regression_test(buildings_csv buildings_csv)

# ===========================================================================
# buildings_terrain/ – Buildings on Gaussian hill (terrain-aligned)
# ===========================================================================
add_regression_test(buildings_terrain buildings_terrain)

# ===========================================================================
# wake_single_building/ – Single building wake model (Röckle formulation)
# ===========================================================================
add_regression_test(wake_single_building wake_single_building)

# ===========================================================================
# Advanced features
# Building Wake and Enhanced Features
# ===========================================================================

# ---------------------------------------------------------------------------
# rooftop_vortex/ – Rooftop vortex parameterization in building cavity zones
# ---------------------------------------------------------------------------
add_regression_test(rooftop_vortex rooftop_vortex)

# ---------------------------------------------------------------------------
# building_oriented/ – Arbitrary building orientation effects on wake model
# ---------------------------------------------------------------------------
add_regression_test(building_oriented building_oriented)

# ---------------------------------------------------------------------------
# building_array/ – Building array configurations
# ---------------------------------------------------------------------------
add_regression_test(building_array building_array)

# ---------------------------------------------------------------------------
# porous_buildings/ – Building porosity model for porous structures
# ---------------------------------------------------------------------------
add_regression_test(porous_buildings porous_buildings)

# ---------------------------------------------------------------------------
# multiheight_extraction/ – Multi-height wind field extraction (new feature)
# ---------------------------------------------------------------------------
add_regression_test(multiheight_extraction multiheight_extraction)

# ===========================================================================
# Wall function tests
# ===========================================================================

# ---------------------------------------------------------------------------
# wall_function_flat/ – Basic wall function test on flat terrain
# ---------------------------------------------------------------------------
add_regression_test(wall_function_flat wall_function_flat)

# ---------------------------------------------------------------------------
# wall_function_stable/ – Wall function with stable stability corrections
# ---------------------------------------------------------------------------
add_regression_test(wall_function_stable wall_function_stable)

# ---------------------------------------------------------------------------
# wall_function_unstable/ – Wall function with unstable stability corrections
# ---------------------------------------------------------------------------
add_regression_test(wall_function_unstable wall_function_unstable)

# ---------------------------------------------------------------------------
# wall_function_adaptive/ – Wall function with adaptive activation
# ---------------------------------------------------------------------------
add_regression_test(wall_function_adaptive wall_function_adaptive)
# ---------------------------------------------------------------------------
# Thermal Stratification and Kinematic BC tests
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# buoyancy_stratification/ – Thermal stratification with buoyancy effects
# ---------------------------------------------------------------------------
add_regression_test(buoyancy_stratification buoyancy_stratification)

# ---------------------------------------------------------------------------
# buoyancy_rhs/ – Buoyancy RHS method (source term in lambda equation)
# ---------------------------------------------------------------------------
add_regression_test(buoyancy_rhs buoyancy_rhs)

# ---------------------------------------------------------------------------
# terrain_kinematic_bc/ – Kinematic terrain-following boundary condition
# ---------------------------------------------------------------------------
add_regression_test(terrain_kinematic_bc terrain_kinematic_bc)

# ---------------------------------------------------------------------------
# combined_thermal_terrain/ – Combined buoyancy and kinematic BC features
# ---------------------------------------------------------------------------
add_regression_test(combined_thermal_terrain combined_thermal_terrain)

# ---------------------------------------------------------------------------
# Canopy model tests
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# canopy_forest/ – Forest canopy with MacDonald et al. (2000) parameterization
# ---------------------------------------------------------------------------
add_regression_test(canopy_forest canopy_forest)

# ---------------------------------------------------------------------------
# canopy_exponential/ – Exponential canopy profile model
# ---------------------------------------------------------------------------
add_regression_test(canopy_exponential canopy_exponential)

# ---------------------------------------------------------------------------
# Surface Flux Diagnostics: Drag Coefficient Diagnostic
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# drag_diagnostic/ – Drag coefficient and heat flux diagnostics with varying roughness
# ---------------------------------------------------------------------------
add_regression_test(drag_diagnostic drag_diagnostic)

# ---------------------------------------------------------------------------
# Stability Corrections: Non-Neutral Stability Corrections (Businger-Dyer)
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# stability_stable/ – Stable atmospheric conditions with L > 0
# ---------------------------------------------------------------------------
add_regression_test(stability_stable stability_stable)

# ---------------------------------------------------------------------------
# stability_unstable/ – Unstable atmospheric conditions with L < 0
# ---------------------------------------------------------------------------
add_regression_test(stability_unstable stability_unstable)

# ---------------------------------------------------------------------------
# ekman_veer/ – Ekman spiral wind veer correction
# ---------------------------------------------------------------------------
add_regression_test(ekman_veer ekman_veer)

# ---------------------------------------------------------------------------
# Anisotropy and Refinement: Height-Dependent Anisotropy
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# alphav_height/ – Height-dependent vertical anisotropy coefficient
# ---------------------------------------------------------------------------
add_regression_test(alphav_height alphav_height)

# ---------------------------------------------------------------------------
# Elevation and Scaling: Elevation-Dependent Wind Scaling
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# elevation_scaling/ – Terrain elevation effects on wind speed
# ---------------------------------------------------------------------------
add_regression_test(elevation_scaling elevation_scaling)

# ---------------------------------------------------------------------------
# Transient Effects: Time-Varying Boundary Conditions
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# time_varying/ – Time-varying wind boundary conditions
# ---------------------------------------------------------------------------
add_regression_test(time_varying time_varying)

# ===========================================================================
# New Features (Very Easy 1-5)
# ===========================================================================

# ---------------------------------------------------------------------------
# divergence_source/ – Feature 1: Divergence source terms for convective plumes
# ---------------------------------------------------------------------------
add_regression_test(divergence_source divergence_source)

# ---------------------------------------------------------------------------
# holtslag_stability/ – Feature 2: Holtslag-De Bruin stability functions
# ---------------------------------------------------------------------------
add_regression_test(holtslag_stability holtslag_stability)

# ---------------------------------------------------------------------------
# landuse_powerlaw/ – Feature 3: Land use-based power-law exponents
# ---------------------------------------------------------------------------
add_regression_test(landuse_powerlaw landuse_powerlaw)

# ---------------------------------------------------------------------------
# diurnal_temperature/ – Feature 4: Diurnal temperature profile
# ---------------------------------------------------------------------------
add_regression_test(diurnal_temperature diurnal_temperature)

# ---------------------------------------------------------------------------
# vegetation_roughness/ – Feature 5: Vegetation attenuation factor for roughness
# ---------------------------------------------------------------------------
add_regression_test(vegetation_roughness vegetation_roughness)

# ---------------------------------------------------------------------------
# katabatic_flow/ – Feature 6: Katabatic/anabatic slope flows parameterization
# ---------------------------------------------------------------------------
add_regression_test(katabatic_flow katabatic_flow)

# ---------------------------------------------------------------------------
# wake_huber_snyder/ – Feature 7: Huber-Snyder wake model (alternative to Röckle)
# ---------------------------------------------------------------------------
add_regression_test(wake_huber_snyder wake_huber_snyder)

# ===========================================================================
# Puff Dispersion Tests
# ===========================================================================

# ---------------------------------------------------------------------------
# puff_gaussian/ – Basic Gaussian puff dispersion on uniform wind
# ---------------------------------------------------------------------------
add_regression_test(puff_gaussian puff_gaussian)

# ---------------------------------------------------------------------------
# puff_terrain/ – Puff dispersion on terrain-following wind field
# ---------------------------------------------------------------------------
add_regression_test(puff_terrain puff_terrain)

# ---------------------------------------------------------------------------
# puff_buildings/ – Puff dispersion with building wake effects
# ---------------------------------------------------------------------------
add_regression_test(puff_buildings puff_buildings)

# ---------------------------------------------------------------------------
# puff_canopy/ – Puff dispersion with canopy drag effects
# ---------------------------------------------------------------------------
add_regression_test(puff_canopy puff_canopy)

# ---------------------------------------------------------------------------
# puff_coupled_full/ – Fully coupled puff dispersion with all features
# ---------------------------------------------------------------------------
add_regression_test(puff_coupled_full puff_coupled_full)

# ---------------------------------------------------------------------------
# terrain_following_steep/ – Terrain-following coordinates on steep terrain
# ---------------------------------------------------------------------------
add_regression_test(terrain_following_steep terrain_following_steep)

# ===========================================================================
# Advanced Boundary Conditions & Wind Profile Features
# ===========================================================================

# ---------------------------------------------------------------------------
# diurnal_roughness/ – Diurnal roughness length variations z₀(t)
# ---------------------------------------------------------------------------
add_regression_test(diurnal_roughness diurnal_roughness)

# ---------------------------------------------------------------------------
# bl_decay/ – Exponential boundary layer wind decay
# ---------------------------------------------------------------------------
add_regression_test(bl_decay bl_decay)

# ---------------------------------------------------------------------------
# momentum_flux/ – Momentum flux output fields (τ_x, τ_y, u*)
# ---------------------------------------------------------------------------
add_regression_test(momentum_flux momentum_flux)

# ---------------------------------------------------------------------------
# richardson_diagnostic/ – Richardson number boundary layer depth diagnostic
# ---------------------------------------------------------------------------
add_regression_test(richardson_diagnostic richardson_diagnostic)

# ---------------------------------------------------------------------------
# froude_scaling/ – Froude number height scaling with terrain blocking
# ---------------------------------------------------------------------------
add_regression_test(froude_scaling froude_scaling)

# ---------------------------------------------------------------------------
# ageostrophic_balance/ – Ageostrophic wind balance boundary conditions
# ---------------------------------------------------------------------------
add_regression_test(ageostrophic_balance ageostrophic_balance)

# ===========================================================================
# Surface Flux Diagnostics and Refinement Features
# ===========================================================================

# ---------------------------------------------------------------------------
# flux_diagnostics_feature/ – Surface flux diagnostics (SHF, LHF, momentum flux, drag coefficient)
# ---------------------------------------------------------------------------
add_regression_test(flux_diagnostics_feature flux_diagnostics_feature)

# ---------------------------------------------------------------------------
# landuse_classification/ – NLCD-compatible land-use classification and z0 lookup
# ---------------------------------------------------------------------------
add_regression_test(landuse_classification landuse_classification)

# ---------------------------------------------------------------------------
# directional_bias_correction/ – Directional and speed bias correction for NWP models
# ---------------------------------------------------------------------------
add_regression_test(directional_bias_correction directional_bias_correction)

# ---------------------------------------------------------------------------
# Phase 4: Model Parameter Adaptive Systems
# ---------------------------------------------------------------------------

# phase4_wake_blending/ – Feature 25: Wake deficit superposition refinement
add_regression_test(phase4_wake_blending phase4_wake_blending)

# phase4_ri_b_stability/ – Feature 26: Conditional stability model selection
add_regression_test(phase4_ri_b_stability phase4_ri_b_stability)

# phase4_orographic_thresholds/ – Feature 27: Orographic model activation thresholds
add_regression_test(phase4_orographic_thresholds phase4_orographic_thresholds)

# ---------------------------------------------------------------------------
# Custom target: make regtest
# ---------------------------------------------------------------------------
add_custom_target(regtest
                  COMMAND ${CMAKE_CTEST_COMMAND} -L regtest --output-on-failure
                  COMMENT "Running all regression tests"
                  USES_TERMINAL)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Regression tests:")
message(STATUS "  flat_terrain/          : mass-consistent solver on flat terrain (dx=dy=dz=50 m, 2x2x2 grid)")
message(STATUS "  gaussian_hill/         : terrain-following wind on a Gaussian hill (dx=dy=30 m, dz=25 m)")
message(STATUS "  gaussian_hill_weno5/   : Gaussian hill with WENO-5 derivatives (dx=dy=30 m, dz=25 m)")
message(STATUS "  raws_synthetic/        : Gaussian hill with RAWS wind initialization (IDW interpolation)")
message(STATUS "  surface_data_synthetic/: Gaussian hill with surface_data initialization (HRRR-style)")
message(STATUS "  uniform_wind/          : Gaussian hill with uniform wind initialization")
message(STATUS "  powerlaw_flat/         : Power-law wind profile on flat terrain")
message(STATUS "  powerlaw_gaussian/     : Power-law wind profile on Gaussian hill")
message(STATUS "  powerlaw_building_flat/: Power-law profile with buildings on flat terrain")
message(STATUS "  powerlaw_building_terrain/: Power-law profile with buildings on terrain")
message(STATUS "  buildings_flat/        : Buildings (union of boxes) on flat terrain")
message(STATUS "  buildings_csv/         : Buildings from CSV file on flat terrain")
message(STATUS "  buildings_terrain/     : Buildings on Gaussian hill (terrain-aligned)")
message(STATUS "  wake_single_building/  : Single building wake model (Röckle formulation)")
message(STATUS "  rooftop_vortex/        : Rooftop vortex in building cavity zones")
message(STATUS "  building_oriented/     : Arbitrary building orientation effects")
message(STATUS "  building_array/        : Building array configurations")
message(STATUS "  porous_buildings/      : Building porosity model for porous structures")
message(STATUS "  multiheight_extraction/: Multi-height wind field extraction (10m, 50m, 100m)")
message(STATUS "  wall_function_flat/    : Wall function on flat terrain")
message(STATUS "  wall_function_stable/  : Wall function with stable atmospheric corrections")
message(STATUS "  wall_function_unstable/: Wall function with unstable atmospheric corrections")
message(STATUS "  wall_function_adaptive/: Wall function with adaptive grid resolution activation")
message(STATUS "  buoyancy_stratification/: Thermal stratification with buoyancy effects")
message(STATUS "  buoyancy_rhs/          : Buoyancy RHS method (source term in lambda equation)")
message(STATUS "  terrain_kinematic_bc/  : Kinematic terrain-following boundary condition")
message(STATUS "  combined_thermal_terrain/: Combined buoyancy and kinematic BC features")
message(STATUS "  canopy_forest/         : Forest canopy with MacDonald et al. (2000) parameterization")
message(STATUS "  canopy_exponential/    : Exponential canopy profile model")
message(STATUS "  drag_diagnostic/       : Drag coefficient and heat flux diagnostics")
message(STATUS "  stability_stable/      : Non-neutral stability (stable conditions, L > 0)")
message(STATUS "  stability_unstable/    : Non-neutral stability (unstable conditions, L < 0)")
message(STATUS "  ekman_veer/            : Ekman spiral wind veer correction (Coriolis effects)")
message(STATUS "  alphav_height/         : Height-dependent vertical anisotropy coefficient")
message(STATUS "  elevation_scaling/     : Terrain elevation effects on wind speed")
message(STATUS "  time_varying/          : Time-varying wind boundary conditions")
message(STATUS "  divergence_source/     : Divergence source terms for convective plumes (Feature 1)")
message(STATUS "  holtslag_stability/    : Holtslag-De Bruin stability functions (Feature 2)")
message(STATUS "  landuse_powerlaw/      : Land use-based power-law exponents (Feature 3)")
message(STATUS "  diurnal_temperature/   : Diurnal temperature profile (Feature 4)")
message(STATUS "  vegetation_roughness/  : Vegetation attenuation factor for roughness (Feature 5)")
message(STATUS "  puff_gaussian/         : Basic Gaussian puff dispersion on uniform wind")
message(STATUS "  puff_terrain/          : Puff dispersion on terrain-following wind field")
message(STATUS "  puff_buildings/        : Puff dispersion with building wake effects")
message(STATUS "  puff_canopy/           : Puff dispersion with canopy drag effects")
message(STATUS "  puff_coupled_full/     : Fully coupled puff dispersion with all features")
message(STATUS "  terrain_following_steep/ : Terrain-following coordinates on steep terrain (Feature 17)")
message(STATUS "  diurnal_roughness/     : Diurnal roughness length variations z₀(t)")
message(STATUS "  bl_decay/              : Exponential boundary layer wind decay above BL depth")
message(STATUS "  momentum_flux/         : Momentum flux output fields (τ_x, τ_y, u*)")
message(STATUS "  richardson_diagnostic/ : Richardson number boundary layer depth diagnostic")
message(STATUS "  froude_scaling/        : Froude number height scaling with terrain blocking")
message(STATUS "  ageostrophic_balance/  : Ageostrophic wind balance boundary conditions")
message(STATUS "")
message(STATUS "Core Surface and Wind Profile Features (Surface Flux Diagnostics and Refinement):")
message(STATUS "  flux_diagnostics_feature/       : Surface flux diagnostics (SHF, LHF, momentum flux, Cd)")
message(STATUS "  landuse_classification/         : NLCD land-use classification and z0 lookup")
message(STATUS "  directional_bias_correction/    : Wind direction and speed bias correction")
message(STATUS "")
message(STATUS "Phase 4: Model Parameter Adaptive Systems")
message(STATUS "  phase4_wake_blending/           : Feature 25 - Wake deficit superposition refinement")
message(STATUS "  phase4_ri_b_stability/          : Feature 26 - Conditional stability model selection")
message(STATUS "  phase4_orographic_thresholds/   : Feature 27 - Orographic model activation thresholds")
message(STATUS "")
message(STATUS "Run tests with: ctest -L regtest  or  cmake --build build --target regtest")
