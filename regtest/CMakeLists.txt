# CMakeLists.txt for regression tests
# Tests are organised into sub-folders:
#   flat_terrain/  – flat domain, verifies mass consistency on trivial geometry
#   gaussian_hill/ – Gaussian hill terrain, verifies terrain-following wind field

cmake_minimum_required(VERSION 3.20)
enable_testing()
find_package(Python3 COMPONENTS Interpreter REQUIRED)

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

# ---------------------------------------------------------------------------
# Helper: add a Python-based regression test
# ---------------------------------------------------------------------------
function(add_python_regression_test TEST_NAME TEST_DIR TEST_SCRIPT)
    set(TEST_INPUT    "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/inputs.i")
    set(TEST_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TEST_DIR}")
    file(MAKE_DIRECTORY ${TEST_WORK_DIR})

    # Copy terrain/data CSV and JSON files to the test working directory
    file(GLOB _DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*.csv" "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*.json")
    foreach(_df ${_DATA_FILES})
        file(COPY "${_df}" DESTINATION ${TEST_WORK_DIR})
    endforeach()
    
    add_test(NAME ${TEST_NAME}
             COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/${TEST_SCRIPT}" 
                     ${TEST_INPUT} ${TEST_WORK_DIR}
             WORKING_DIRECTORY ${TEST_WORK_DIR})
    set_tests_properties(${TEST_NAME} PROPERTIES
                        TIMEOUT 600
                        LABELS  "regtest")
endfunction()

# ===========================================================================
# flat_terrain/ – flat domain (all z = 0), trivial mass-consistency check
# ===========================================================================
add_regression_test(flat_terrain flat_terrain)

# ===========================================================================
# flatirons_turbines/ – Sloping Flatirons NREL terrain with wind turbines
# ===========================================================================
add_regression_test(flatirons_turbines flatirons_turbines)

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
# topographic_shielding/ – central ridge with topographic barrier shielding
# ===========================================================================
add_regression_test(topographic_shielding topographic_shielding)

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
# canopy_spatially_varying/ – Spatially-varying forest canopy parameters
# ---------------------------------------------------------------------------
add_regression_test(canopy_spatially_varying canopy_spatially_varying)

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
# Advanced Diagnostic and Physical Parameterization Features
# ===========================================================================

# ---------------------------------------------------------------------------
# divergence_source/ – Divergence source terms for convective plumes
# ---------------------------------------------------------------------------
add_regression_test(divergence_source divergence_source)

# ---------------------------------------------------------------------------
# holtslag_stability/ – Holtslag-De Bruin stability functions
# ---------------------------------------------------------------------------
add_regression_test(holtslag_stability holtslag_stability)

# ---------------------------------------------------------------------------
# landuse_powerlaw/ – Land use-based power-law exponents
# ---------------------------------------------------------------------------
add_regression_test(landuse_powerlaw landuse_powerlaw)

# ---------------------------------------------------------------------------
# diurnal_temperature/ – Diurnal temperature profile
# ---------------------------------------------------------------------------
add_regression_test(diurnal_temperature diurnal_temperature)

# ---------------------------------------------------------------------------
# vegetation_roughness/ – Vegetation attenuation factor for roughness
# ---------------------------------------------------------------------------
add_regression_test(vegetation_roughness vegetation_roughness)

# ---------------------------------------------------------------------------
# katabatic_flow/ – Katabatic/anabatic slope flows parameterization
# ---------------------------------------------------------------------------
add_regression_test(katabatic_flow katabatic_flow)

# ---------------------------------------------------------------------------
# wake_huber_snyder/ – Huber-Snyder wake model (alternative to Röckle)
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
# puff_deposition/ – Puff dispersion with dry deposition on ground
# ---------------------------------------------------------------------------
add_regression_test(puff_deposition puff_deposition)

# ---------------------------------------------------------------------------
# puff_scavenging_decay/ – Puff wet deposition and dynamic decay
# ---------------------------------------------------------------------------
add_regression_test(puff_scavenging_decay puff_scavenging_decay)

# ---------------------------------------------------------------------------
# puff_coupled_full/ – Fully coupled puff dispersion with all features
# ---------------------------------------------------------------------------
add_regression_test(puff_coupled_full puff_coupled_full)

# ---------------------------------------------------------------------------
# puff_downwash_single_time/ – Single time instance cavity trapping test
# ---------------------------------------------------------------------------
add_regression_test(puff_downwash_single_time puff_downwash_single_time)

# ---------------------------------------------------------------------------
# puff_downwash_multi_time/ – Multiple time instances for wake recirculation
# ---------------------------------------------------------------------------
add_regression_test(puff_downwash_multi_time puff_downwash_multi_time)

# ---------------------------------------------------------------------------
# puff_aloha_features/ – ALOHA-style Infiltration, TV emissions, and Threat Zones
# ---------------------------------------------------------------------------
add_regression_test(puff_aloha_features puff_aloha_features)

# ---------------------------------------------------------------------------
# lpdm_terrain/ – Lagrangian Particle Dispersion Model on terrain
# ---------------------------------------------------------------------------
add_regression_test(lpdm_terrain lpdm_terrain)

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
# Adaptive parameterization features
# ---------------------------------------------------------------------------

# wake_blending_adaptive/ – Wake deficit superposition refinement
add_regression_test(wake_blending_adaptive wake_blending_adaptive)

# ri_b_stability_selection/ – Conditional stability model selection
add_regression_test(ri_b_stability_selection ri_b_stability_selection)

# orographic_activation_thresholds/ – Orographic model activation thresholds
add_regression_test(orographic_activation_thresholds orographic_activation_thresholds)

# ===========================================================================
# synthetic_turbulence_full/ – Complete synthetic turbulence pipeline
# ===========================================================================
# Tests the full synthetic turbulence framework:
# - Turbulence parameter parsing and spectral model selection
# - Random field synthesis from spectral parameters
# - Time-series generation with temporal correlations
# - BTS Export: OpenFAST-compatible binary format output
# ============================================================================= 
add_regression_test(synthetic_turbulence_full synthetic_turbulence_full)
add_python_regression_test(synthetic_turbulence_full_validation synthetic_turbulence_full
                          test_synthetic_turbulence.py)

# ===========================================================================
# IEC 61400-1:2019 Spectral Synthesis Tests
# ===========================================================================
# Tests IEC 61400-1 turbulence categories with different spectral models
# and coherence functions in the C++ wind solver

# -----------------------------------------------------------------------
# iec61400_von_karman/ – IEC Category B with Von Kármán spectrum
# -----------------------------------------------------------------------
# Validates IEC61400 intensity model parser and Von Kármán spectrum synthesis
add_regression_test(iec61400_von_karman iec61400_von_karman)

# -----------------------------------------------------------------------
# iec61400_kaimal/ – IEC Category A with Kaimal spectrum
# -----------------------------------------------------------------------
# Validates Kaimal spectrum model and PowerLaw coherence model
add_regression_test(iec61400_kaimal iec61400_kaimal)

# -----------------------------------------------------------------------
# iec61400_category_c/ – IEC Category C with QuadraticExponential coherence
# -----------------------------------------------------------------------
# Validates low-turbulence sites (Category C) and QuadraticExponential coherence
add_regression_test(iec61400_category_c iec61400_category_c)

# -----------------------------------------------------------------------
# iec61400_categories/ – Python-based regression tests
# -----------------------------------------------------------------------
# Python unit tests for IEC 61400 spectral models across all categories
add_python_regression_test(iec61400_categories_regtest iec61400_categories
                          test_iec61400_categories.py)

# ===========================================================================
# field_output_test/ – Unified field output (21 diagnostic fields)
# ===========================================================================
add_regression_test(field_output_test field_output_test)

# ===========================================================================
# New easy-to-implement features tests
# ===========================================================================
add_regression_test(pasquill_gifford pasquill_gifford)
add_regression_test(power_law_profile_bl power_law_profile_bl)
add_regression_test(deaves_harris deaves_harris)
add_regression_test(capping_lid capping_lid)

# ===========================================================================
# High-Yield Parameterizations (Surface Roughness, Coriolis, Windbreaks)
# ===========================================================================
add_regression_test(canopy_lai_profiles canopy_lai_profiles)
add_regression_test(coriolis_meridional coriolis_meridional)
add_regression_test(subgrid_windbreaks subgrid_windbreaks)

# ===========================================================================
# parameter_sensitivity_test/ – Parameter sensitivity sweep tool
# ===========================================================================
add_python_regression_test(parameter_sensitivity_test parameter_sensitivity_test 
                          test_parameter_sensitivity.py)

# ===========================================================================
# Analytical Wake & Turbulence Extensions (TurbOPark, Jimenez, Crespo-Hernandez)
# ===========================================================================
if(MASSCONSISTENT_BUILD_PYTHON_BINDINGS)
  add_python_regression_test(analytical_wake_extensions analytical_wake_extensions
                            test_phase2_analytical_wake_extensions.py)
  add_python_regression_test(analytical_wake_new_features analytical_wake_new_features
                            test_analytical_wake_new_features.py)
  add_python_regression_test(aep_dispersion aep_dispersion
                            test_aep_dispersion.py)
  add_python_regression_test(phase3_advanced_wake_models phase3_advanced_wake_models
                            test_phase3_advanced_wake_models.py)
  add_python_regression_test(bastankhah_deflection bastankhah_deflection
                            test_bastankhah_deflection.py)
  add_python_regression_test(wildfire_levelset_coupling wildfire_levelset_coupling
                            test_wildfire_coupling.py)
else()
  message(STATUS "  analytical_wake_extensions: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  aep_dispersion: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  phase3_advanced_wake_models: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  wildfire_levelset_coupling: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
endif()

# ===========================================================================
# netcdf_ingestion/ – NetCDF 3D wind parsing and ingestion regtests
# ===========================================================================
add_regression_test(netcdf_cpp_ingestion netcdf_cpp_ingestion)
add_python_regression_test(netcdf_ingestion_regtest netcdf_ingestion 
                          test_netcdf_ingestion.py)

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
message(STATUS "  topographic_shielding/ : Central ridge with topographic barrier shielding (IDW interpolation)")
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
message(STATUS "  canopy_spatially_varying/: Spatially-varying canopy height and frontal area index")
message(STATUS "  canopy_exponential/    : Exponential canopy profile model")
message(STATUS "  canopy_lai_profiles/   : Vertical Leaf Area Index (LAI) profiles")
message(STATUS "  coriolis_meridional/   : Coriolis Meridional Beta-Effect and scaled Ekman veer")
message(STATUS "  subgrid_windbreaks/    : Sub-grid windbreaks and directional barrier drag")
message(STATUS "  drag_diagnostic/       : Drag coefficient and heat flux diagnostics")
message(STATUS "  stability_stable/      : Non-neutral stability (stable conditions, L > 0)")
message(STATUS "  stability_unstable/    : Non-neutral stability (unstable conditions, L < 0)")
message(STATUS "  ekman_veer/            : Ekman spiral wind veer correction (Coriolis effects)")
message(STATUS "  alphav_height/         : Height-dependent vertical anisotropy coefficient")
message(STATUS "  elevation_scaling/     : Terrain elevation effects on wind speed")
message(STATUS "  time_varying/          : Time-varying wind boundary conditions")
message(STATUS "  divergence_source/     : Divergence source terms for convective plumes")
message(STATUS "  holtslag_stability/    : Holtslag-De Bruin stability functions")
message(STATUS "  landuse_powerlaw/      : Land use-based power-law exponents")
message(STATUS "  diurnal_temperature/   : Diurnal temperature profile")
message(STATUS "  vegetation_roughness/  : Vegetation attenuation factor for roughness")
message(STATUS "  puff_gaussian/         : Basic Gaussian puff dispersion on uniform wind")
message(STATUS "  puff_terrain/          : Puff dispersion on terrain-following wind field")
message(STATUS "  puff_buildings/        : Puff dispersion with building wake effects")
message(STATUS "  puff_canopy/           : Puff dispersion with canopy drag effects")
message(STATUS "  puff_coupled_full/     : Fully coupled puff dispersion with all features")
message(STATUS "  puff_downwash_single_time/ : Single time step cavity trapping (AERMOD PRIME)")
message(STATUS "  puff_downwash_multi_time/  : Multiple time steps wake recirculation and plume deformation")
message(STATUS "  lpdm_terrain/          : Lagrangian Particle Dispersion Model on terrain")
message(STATUS "  terrain_following_steep/ : Terrain-following coordinates on steep terrain")
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
message(STATUS "Adaptive Parameterization Features")
message(STATUS "  wake_blending_adaptive/         : Wake deficit superposition refinement")
message(STATUS "  ri_b_stability_selection/       : Conditional stability model selection")
message(STATUS "  orographic_activation_thresholds/ : Orographic model activation thresholds")
message(STATUS "")
message(STATUS "Output & Integration Features")
message(STATUS "  synthetic_turbulence_full/      : synthetic turbulence pipeline (spectral synthesis + BTS export)")
message(STATUS "  iec61400_von_karman/            : IEC 61400-1:2019 Category B with Von Kármán spectrum")
message(STATUS "  iec61400_kaimal/                : IEC 61400-1:2019 Category A with Kaimal spectrum")
message(STATUS "  iec61400_category_c/            : IEC 61400-1:2019 Category C with QuadraticExponential coherence")
message(STATUS "  iec61400_categories_regtest/    : Python unit tests for IEC 61400 spectral models (20 tests)")
message(STATUS "  field_output_test/              : Unified field output with 21 diagnostic fields")
message(STATUS "  parameter_sensitivity_test/     : Parameter sensitivity sweep tool validation")
message(STATUS "  analytical_wake_extensions: Analytical turbine wake deficit, deflection, and added turbulence models")
message(STATUS "")
message(STATUS "Run tests with: ctest -L regtest  or  cmake --build build --target regtest")
