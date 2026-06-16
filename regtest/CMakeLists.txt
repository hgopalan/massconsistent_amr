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

    # Copy terrain/data CSV/FSL/UP files to the working directory
    file(GLOB _DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*.csv" "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*.fsl" "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*.up")
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
add_regression_test(flat_terrain terrain/flat_terrain)
add_regression_test(wire_loading terrain/wire_loading)
add_regression_test(bridge_loading terrain/bridge_loading)

# ===========================================================================
# flatirons_turbines/ – Sloping Flatirons NREL terrain with wind turbines
# ===========================================================================
add_regression_test(flatirons_turbines terrain/flatirons_turbines)

# ===========================================================================
# gaussian_hill/ – Gaussian hill terrain, terrain-following log-law wind
# ===========================================================================
add_regression_test(gaussian_hill terrain/gaussian_hill)

# ===========================================================================
# gaussian_hill_weno5/ – Gaussian hill with WENO-5 derivatives
# ===========================================================================
add_regression_test(gaussian_hill_weno5 terrain/gaussian_hill_weno5)

# ===========================================================================
# raws_synthetic/ – Gaussian hill with RAWS wind initialization (IDW interp)
# ===========================================================================
add_regression_test(raws_synthetic terrain/raws_synthetic)

# ===========================================================================
# calmet_idw/ – CALMET-style IDW interpolation features test
# ===========================================================================
add_regression_test(calmet_idw terrain/calmet_idw)

# ===========================================================================
# topographic_shielding/ – central ridge with topographic barrier shielding
# ===========================================================================
add_regression_test(topographic_shielding terrain/topographic_shielding)

# ===========================================================================
# surface_data_synthetic/ – Gaussian hill with surface_data initialization
# ===========================================================================
add_regression_test(surface_data_synthetic terrain/surface_data_synthetic)

# ===========================================================================
# uniform_wind/ – Gaussian hill with uniform wind initialization
# ===========================================================================
add_regression_test(uniform_wind terrain/uniform_wind)

# ===========================================================================
# powerlaw_flat/ – Power-law wind profile on flat terrain
# ===========================================================================
add_regression_test(powerlaw_flat terrain/powerlaw_flat)

# ===========================================================================
# powerlaw_gaussian/ – Power-law wind profile on Gaussian hill
# ===========================================================================
add_regression_test(powerlaw_gaussian terrain/powerlaw_gaussian)

# ===========================================================================
# powerlaw_building_flat/ – Power-law profile with buildings on flat terrain
# ===========================================================================
add_regression_test(powerlaw_building_flat terrain/powerlaw_building_flat)

# ===========================================================================
# powerlaw_building_terrain/ – Power-law profile with buildings on terrain
# ===========================================================================
add_regression_test(powerlaw_building_terrain terrain/powerlaw_building_terrain)

# ===========================================================================
# buildings_flat/ – Buildings (union of boxes) on flat terrain
# ===========================================================================
add_regression_test(buildings_flat buildings/buildings_flat)

# ===========================================================================
# buildings_csv/ – Buildings from CSV file on flat terrain
# ===========================================================================
add_regression_test(buildings_csv buildings/buildings_csv)

# ===========================================================================
# urban_layout/ – Regular box, L-shaped, T-shaped, U-shaped and polygonal buildings
# ===========================================================================
add_regression_test(urban_layout buildings/urban_layout)

# ===========================================================================
# buildings_terrain/ – Buildings on Gaussian hill (terrain-aligned)
# ===========================================================================
add_regression_test(buildings_terrain buildings/buildings_terrain)

# ===========================================================================
# wake_single_building/ – Single building wake model (Röckle formulation)
# ===========================================================================
add_regression_test(wake_single_building wakes/wake_single_building)
add_regression_test(wake_non_rectangular_sloped wakes/wake_non_rectangular_sloped)
add_regression_test(wake_complex_gaussian_hills wakes/wake_complex_gaussian_hills)

# ===========================================================================
# Advanced features
# Building Wake and Enhanced Features
# ===========================================================================

# ---------------------------------------------------------------------------
# rooftop_vortex/ – Rooftop vortex parameterization in building cavity zones
# ---------------------------------------------------------------------------
add_regression_test(rooftop_vortex wakes/rooftop_vortex)

# ---------------------------------------------------------------------------
# building_oriented/ – Arbitrary building orientation effects on wake model
# ---------------------------------------------------------------------------
add_regression_test(building_oriented buildings/building_oriented)

# ---------------------------------------------------------------------------
# building_array/ – Building array configurations
# ---------------------------------------------------------------------------
add_regression_test(building_array buildings/building_array)

# ---------------------------------------------------------------------------
# porous_buildings/ – Building porosity model for porous structures
# ---------------------------------------------------------------------------
add_regression_test(porous_buildings buildings/porous_buildings)

# ---------------------------------------------------------------------------
# eb_solid_cells/ – Embedded Boundary (EB) solid cell masking regression test
# ---------------------------------------------------------------------------
add_python_regression_test(eb_solid_cells buildings/eb_solid_cells verify_eb.py)

# ---------------------------------------------------------------------------
# multiheight_extraction/ – Multi-height wind field extraction (new feature)
# ---------------------------------------------------------------------------
add_regression_test(multiheight_extraction diagnostics/multiheight_extraction)

# ===========================================================================
# Wall function tests
# ===========================================================================

# ---------------------------------------------------------------------------
# wall_function_flat/ – Basic wall function test on flat terrain
# ---------------------------------------------------------------------------
add_regression_test(wall_function_flat physics/wall_function_flat)

# ---------------------------------------------------------------------------
# wall_function_stable/ – Wall function with stable stability corrections
# ---------------------------------------------------------------------------
add_regression_test(wall_function_stable physics/wall_function_stable)

# ---------------------------------------------------------------------------
# wall_function_unstable/ – Wall function with unstable stability corrections
# ---------------------------------------------------------------------------
add_regression_test(wall_function_unstable physics/wall_function_unstable)

# ---------------------------------------------------------------------------
# wall_function_adaptive/ – Wall function with adaptive activation
# ---------------------------------------------------------------------------
add_regression_test(wall_function_adaptive physics/wall_function_adaptive)
# ---------------------------------------------------------------------------
# Thermal Stratification and Kinematic BC tests
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# buoyancy_stratification/ – Thermal stratification with buoyancy effects
# ---------------------------------------------------------------------------
add_regression_test(buoyancy_stratification physics/buoyancy_stratification)

# ---------------------------------------------------------------------------
# buoyancy_rhs/ – Buoyancy RHS method (source term in lambda equation)
# ---------------------------------------------------------------------------
add_regression_test(buoyancy_rhs physics/buoyancy_rhs)

# ---------------------------------------------------------------------------
# terrain_kinematic_bc/ – Kinematic terrain-following boundary condition
# ---------------------------------------------------------------------------
add_regression_test(terrain_kinematic_bc physics/terrain_kinematic_bc)

# ---------------------------------------------------------------------------
# scalar_transport/ – 3D scalar transport with mixing length turbulence model
# ---------------------------------------------------------------------------
add_regression_test(scalar_transport physics/scalar_transport)
add_regression_test(scalar_transport_segregated physics/scalar_transport_segregated)
add_regression_test(scalar_transport_coupled physics/scalar_transport_coupled)

# ---------------------------------------------------------------------------
# combined_thermal_terrain/ – Combined buoyancy and kinematic BC features
# ---------------------------------------------------------------------------
add_regression_test(combined_thermal_terrain physics/combined_thermal_terrain)

# ---------------------------------------------------------------------------
# Canopy model tests
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# canopy_forest/ – Forest canopy with MacDonald et al. (2000) parameterization
# ---------------------------------------------------------------------------
add_regression_test(canopy_forest physics/canopy_forest)

# ---------------------------------------------------------------------------
# canopy_spatially_varying/ – Spatially-varying forest canopy parameters
# ---------------------------------------------------------------------------
add_regression_test(canopy_spatially_varying physics/canopy_spatially_varying)

# ---------------------------------------------------------------------------
# canopy_exponential/ – Exponential canopy profile model
# ---------------------------------------------------------------------------
add_regression_test(canopy_exponential physics/canopy_exponential)

# ---------------------------------------------------------------------------
# morphometric_models/ – Urban morphological parameter estimators
# ---------------------------------------------------------------------------
add_regression_test(morphometric_models physics/morphometric_models)

# ---------------------------------------------------------------------------
# Surface Flux Diagnostics: Drag Coefficient Diagnostic
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# drag_diagnostic/ – Drag coefficient and heat flux diagnostics with varying roughness
# ---------------------------------------------------------------------------
add_regression_test(drag_diagnostic physics/drag_diagnostic)

# ---------------------------------------------------------------------------
# Stability Corrections: Non-Neutral Stability Corrections (Businger-Dyer)
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# stability_stable/ – Stable atmospheric conditions with L > 0
# ---------------------------------------------------------------------------
add_regression_test(stability_stable physics/stability_stable)

# ---------------------------------------------------------------------------
# stability_unstable/ – Unstable atmospheric conditions with L < 0
# ---------------------------------------------------------------------------
add_regression_test(stability_unstable physics/stability_unstable)

# ---------------------------------------------------------------------------
# ekman_veer/ – Ekman spiral wind veer correction
# ---------------------------------------------------------------------------
add_regression_test(ekman_veer physics/ekman_veer)

# ---------------------------------------------------------------------------
# ekman_spiral/ – Analytical Ekman spiral profile initialization
# ---------------------------------------------------------------------------
add_regression_test(ekman_spiral physics/ekman_spiral)

# ---------------------------------------------------------------------------
# Anisotropy and Refinement: Height-Dependent Anisotropy
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# alphav_height/ – Height-dependent vertical anisotropy coefficient
# ---------------------------------------------------------------------------
add_regression_test(alphav_height physics/alphav_height)

# ---------------------------------------------------------------------------
# Elevation and Scaling: Elevation-Dependent Wind Scaling
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# elevation_scaling/ – Terrain elevation effects on wind speed
# ---------------------------------------------------------------------------
add_regression_test(elevation_scaling physics/elevation_scaling)

# ---------------------------------------------------------------------------
# Transient Effects: Time-Varying Boundary Conditions
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# time_varying/ – Time-varying wind boundary conditions
# ---------------------------------------------------------------------------
add_regression_test(time_varying physics/time_varying)

# ===========================================================================
# Advanced Diagnostic and Physical Parameterization Features
# ===========================================================================

# ---------------------------------------------------------------------------
# divergence_source/ – Divergence source terms for convective plumes
# ---------------------------------------------------------------------------
add_regression_test(divergence_source physics/divergence_source)

# ---------------------------------------------------------------------------
# holtslag_stability/ – Holtslag-De Bruin stability functions
# ---------------------------------------------------------------------------
add_regression_test(holtslag_stability physics/holtslag_stability)

# ---------------------------------------------------------------------------
# landuse_powerlaw/ – Land use-based power-law exponents
# ---------------------------------------------------------------------------
add_regression_test(landuse_powerlaw physics/landuse_powerlaw)

# ---------------------------------------------------------------------------
# diurnal_temperature/ – Diurnal temperature profile
# ---------------------------------------------------------------------------
add_regression_test(diurnal_temperature physics/diurnal_temperature)

# ---------------------------------------------------------------------------
# vegetation_roughness/ – Vegetation attenuation factor for roughness
# ---------------------------------------------------------------------------
add_regression_test(vegetation_roughness physics/vegetation_roughness)

# ---------------------------------------------------------------------------
# katabatic_flow/ – Katabatic/anabatic slope flows parameterization
# ---------------------------------------------------------------------------
add_regression_test(katabatic_flow physics/katabatic_flow)

# ---------------------------------------------------------------------------
# wake_huber_snyder/ – Huber-Snyder wake model (alternative to Röckle)
# ---------------------------------------------------------------------------
add_regression_test(wake_huber_snyder wakes/wake_huber_snyder)

# ===========================================================================
# Puff Dispersion Tests
# ===========================================================================

# ---------------------------------------------------------------------------
# puff_gaussian/ – Basic Gaussian puff dispersion on uniform wind
# ---------------------------------------------------------------------------
add_regression_test(puff_gaussian dispersion/puff_gaussian)

# ---------------------------------------------------------------------------
# puff_terrain/ – Puff dispersion on terrain-following wind field
# ---------------------------------------------------------------------------
add_regression_test(puff_terrain dispersion/puff_terrain)

# ---------------------------------------------------------------------------
# puff_buildings/ – Puff dispersion with building wake effects
# ---------------------------------------------------------------------------
add_regression_test(puff_buildings dispersion/puff_buildings)

# ---------------------------------------------------------------------------
# puff_canopy/ – Puff dispersion with canopy drag effects
# ---------------------------------------------------------------------------
add_regression_test(puff_canopy dispersion/puff_canopy)

# ---------------------------------------------------------------------------
# puff_deposition/ – Puff dispersion with dry deposition on ground
# ---------------------------------------------------------------------------
add_regression_test(puff_deposition dispersion/puff_deposition)

# ---------------------------------------------------------------------------
# puff_deposition_terrain/ – Puff wet/dry deposition over Gaussian hill terrain
# ---------------------------------------------------------------------------
add_regression_test(puff_deposition_terrain dispersion/puff_deposition_terrain)

# ---------------------------------------------------------------------------
# puff_scavenging_decay/ – Puff wet deposition and dynamic decay
# ---------------------------------------------------------------------------
add_regression_test(puff_scavenging_decay dispersion/puff_scavenging_decay)

# ---------------------------------------------------------------------------
# puff_coupled_full/ – Fully coupled puff dispersion with all features
# ---------------------------------------------------------------------------
add_regression_test(puff_coupled_full dispersion/puff_coupled_full)

# ---------------------------------------------------------------------------
# puff_downwash_single_time/ – Single time instance cavity trapping test
# ---------------------------------------------------------------------------
add_regression_test(puff_downwash_single_time dispersion/puff_downwash_single_time)

# ---------------------------------------------------------------------------
# puff_downwash_multi_time/ – Multiple time instances for wake recirculation
# ---------------------------------------------------------------------------
add_regression_test(puff_downwash_multi_time dispersion/puff_downwash_multi_time)

# ---------------------------------------------------------------------------
# puff_aloha_features/ – ALOHA-style Infiltration, TV emissions, and Threat Zones
# ---------------------------------------------------------------------------
add_regression_test(puff_aloha_features dispersion/puff_aloha_features)

# ---------------------------------------------------------------------------
# lpdm_terrain/ – Lagrangian Particle Dispersion Model on terrain
# ---------------------------------------------------------------------------
add_regression_test(lpdm_terrain dispersion/lpdm_terrain)

# ---------------------------------------------------------------------------
# lpdm_drift_spatial_lid/ – LPDM with drift correction and spatially-varying capping lid
# ---------------------------------------------------------------------------
add_regression_test(lpdm_drift_spatial_lid dispersion/lpdm_drift_spatial_lid)

# ---------------------------------------------------------------------------
# puff_chemistry_no2_decay/ - NO₂ photochemical decay validation
# =========================================================================
# Tests simple reactive chemistry with 1st-order exponential decay
# Expected: ~50% decay at 4-hour half-life
# ---------------------------------------------------------------------------
add_regression_test(puff_chemistry_no2_decay dispersion/puff_chemistry_no2_decay)

# ---------------------------------------------------------------------------
# puff_chemistry_so2_oxidation/ - SO₂ oxidation to sulfate
# =========================================================================
# Tests SO₂ oxidation with 24-hour half-life and product formation
# Expected: ~71% remaining at 5.56 hours (0.78 half-lives)
# ---------------------------------------------------------------------------
add_regression_test(puff_chemistry_so2_oxidation dispersion/puff_chemistry_so2_oxidation)

# ---------------------------------------------------------------------------
add_python_regression_test(realistic_farm_case dispersion/realistic_farm_case test_realistic_farm_case.py)

# ---------------------------------------------------------------------------
# puff_multispecies_calpuff/ - Multi-species, chemical conversion, deposition, receptors, and line/area/volume sources test
# ---------------------------------------------------------------------------
add_python_regression_test(puff_multispecies_calpuff dispersion/puff_multispecies_calpuff test_multispecies_calpuff.py)

# ---------------------------------------------------------------------------
# terrain_following_steep/ – Terrain-following coordinates on steep terrain
# ---------------------------------------------------------------------------
add_regression_test(terrain_following_steep terrain/terrain_following_steep)

# ===========================================================================
# Advanced Boundary Conditions & Wind Profile Features
# ===========================================================================

# ---------------------------------------------------------------------------
# diurnal_roughness/ – Diurnal roughness length variations z₀(t)
# ---------------------------------------------------------------------------
add_regression_test(diurnal_roughness physics/diurnal_roughness)

# ---------------------------------------------------------------------------
# bl_decay/ – Exponential boundary layer wind decay
# ---------------------------------------------------------------------------
add_regression_test(bl_decay physics/bl_decay)

# ---------------------------------------------------------------------------
# momentum_flux/ – Momentum flux output fields (τ_x, τ_y, u*)
# ---------------------------------------------------------------------------
add_regression_test(momentum_flux physics/momentum_flux)

# ---------------------------------------------------------------------------
# richardson_diagnostic/ – Richardson number boundary layer depth diagnostic
# ---------------------------------------------------------------------------
add_regression_test(richardson_diagnostic physics/richardson_diagnostic)

# ---------------------------------------------------------------------------
# froude_scaling/ – Froude number height scaling with terrain blocking
# ---------------------------------------------------------------------------
add_regression_test(froude_scaling physics/froude_scaling)

# ---------------------------------------------------------------------------
# ageostrophic_balance/ – Ageostrophic wind balance boundary conditions
# ---------------------------------------------------------------------------
add_regression_test(ageostrophic_balance physics/ageostrophic_balance)

# ---------------------------------------------------------------------------
# divergence_damping/ – Divergence damping filter
# ---------------------------------------------------------------------------
add_regression_test(divergence_damping physics/divergence_damping)

# ===========================================================================
# Surface Flux Diagnostics and Refinement Features
# ===========================================================================

# ---------------------------------------------------------------------------
# flux_diagnostics_feature/ – Surface flux diagnostics (SHF, LHF, momentum flux, drag coefficient)
# ---------------------------------------------------------------------------
add_regression_test(flux_diagnostics_feature physics/flux_diagnostics_feature)

# ---------------------------------------------------------------------------
# landuse_classification/ – NLCD-compatible land-use classification and z0 lookup
# ---------------------------------------------------------------------------
add_regression_test(landuse_classification physics/landuse_classification)

# ---------------------------------------------------------------------------
# directional_bias_correction/ – Directional and speed bias correction for NWP models
# ---------------------------------------------------------------------------
add_regression_test(directional_bias_correction physics/directional_bias_correction)

# ---------------------------------------------------------------------------
# Adaptive parameterization features
# ---------------------------------------------------------------------------

# wake_blending_adaptive/ – Wake deficit superposition refinement
add_regression_test(wake_blending_adaptive wakes/wake_blending_adaptive)

# ri_b_stability_selection/ – Conditional stability model selection
add_regression_test(ri_b_stability_selection physics/ri_b_stability_selection)

# orographic_activation_thresholds/ – Orographic model activation thresholds
add_regression_test(orographic_activation_thresholds physics/orographic_activation_thresholds)

# ===========================================================================
# synthetic_turbulence_full/ – Complete synthetic turbulence pipeline
# ===========================================================================
# Tests the full synthetic turbulence framework:
# - Turbulence parameter parsing and spectral model selection
# - Random field synthesis from spectral parameters
# - Time-series generation with temporal correlations
# - BTS Export: OpenFAST-compatible binary format output
# ============================================================================= 
add_regression_test(synthetic_turbulence_full turbulence/synthetic_turbulence_full)
add_python_regression_test(synthetic_turbulence_full_validation turbulence/synthetic_turbulence_full test_synthetic_turbulence.py)

# ===========================================================================
# IEC 61400-1:2019 Spectral Synthesis Tests
# ===========================================================================
# Tests IEC 61400-1 turbulence categories with different spectral models
# and coherence functions in the C++ wind solver

# -----------------------------------------------------------------------
# iec61400_von_karman/ – IEC Category B with Von Kármán spectrum
# -----------------------------------------------------------------------
# Validates IEC61400 intensity model parser and Von Kármán spectrum synthesis
add_regression_test(iec61400_von_karman turbulence/iec61400_von_karman)

# -----------------------------------------------------------------------
# iec61400_kaimal/ – IEC Category A with Kaimal spectrum
# -----------------------------------------------------------------------
# Validates Kaimal spectrum model and PowerLaw coherence model
add_regression_test(iec61400_kaimal turbulence/iec61400_kaimal)

# -----------------------------------------------------------------------
# iec61400_category_c/ – IEC Category C with QuadraticExponential coherence
# -----------------------------------------------------------------------
# Validates low-turbulence sites (Category C) and QuadraticExponential coherence
add_regression_test(iec61400_category_c turbulence/iec61400_category_c)

# -----------------------------------------------------------------------
# iec61400_categories/ – Python-based regression tests
# -----------------------------------------------------------------------
# Python unit tests for IEC 61400 spectral models across all categories
add_python_regression_test(iec61400_categories_regtest turbulence/iec61400_categories test_iec61400_categories.py)

# ===========================================================================
# field_output_test/ – Unified field output (21 diagnostic fields)
# ===========================================================================
add_regression_test(field_output_test diagnostics/field_output_test)

# ===========================================================================
# New easy-to-implement features tests
# ===========================================================================
add_regression_test(sounding_profiles physics/sounding_profiles)
add_regression_test(marine_boundary_layer physics/marine_boundary_layer)
add_regression_test(pasquill_gifford physics/pasquill_gifford)
add_regression_test(power_law_profile_bl physics/power_law_profile_bl)
add_regression_test(deaves_harris physics/deaves_harris)
add_regression_test(capping_lid physics/capping_lid)
add_regression_test(multi_gaussian_hill terrain/multi_gaussian_hill)
add_regression_test(charnock_overwater physics/charnock_overwater)
add_regression_test(precipitation_stability physics/precipitation_stability)
add_regression_test(simplified_richardson_method physics/simplified_richardson_method)
add_python_regression_test(thermodynamic_lid physics/thermodynamic_lid test_thermodynamic_lid.py)
add_regression_test(obrien_adjustment physics/obrien_adjustment)
add_python_regression_test(obrien_adjustment_validation physics/obrien_adjustment verify_obrien.py)

# ===========================================================================
# High-Yield Parameterizations (Surface Roughness, Coriolis, Windbreaks)
# ===========================================================================
add_regression_test(canopy_lai_profiles physics/canopy_lai_profiles)
add_regression_test(coriolis_meridional physics/coriolis_meridional)
add_regression_test(subgrid_windbreaks buildings/subgrid_windbreaks)

# ===========================================================================
# parameter_sensitivity_test/ – Parameter sensitivity sweep tool
# ===========================================================================
add_python_regression_test(parameter_sensitivity_test diagnostics/parameter_sensitivity_test test_parameter_sensitivity.py)

add_python_regression_test(wire_loading_validation terrain/wire_loading verify_wire_loading.py)
add_python_regression_test(bridge_loading_validation terrain/bridge_loading verify_bridge_loading.py)

# ===========================================================================
# Cell-Local Spatially-Varying Variational Anisotropy
# ===========================================================================
add_regression_test(cell_local_anisotropy_base physics/cell_local_anisotropy)
if(MASSCONSISTENT_BUILD_PYTHON_BINDINGS)
  add_python_regression_test(cell_local_anisotropy_python physics/cell_local_anisotropy test_cell_local_anisotropy.py)
endif()

# Cell-Local Anisotropy - All Sources
add_regression_test(cell_local_anisotropy_all_base physics/cell_local_anisotropy_all)
if(MASSCONSISTENT_BUILD_PYTHON_BINDINGS)
  add_python_regression_test(cell_local_anisotropy_all_python physics/cell_local_anisotropy_all test_cell_local_anisotropy_all.py)
endif()

# Cell-Local Anisotropy - Slope Source Only
add_regression_test(cell_local_anisotropy_slope_base physics/cell_local_anisotropy_slope)
if(MASSCONSISTENT_BUILD_PYTHON_BINDINGS)
  add_python_regression_test(cell_local_anisotropy_slope_python physics/cell_local_anisotropy_slope test_cell_local_anisotropy_slope.py)
endif()

# Cell-Local Anisotropy - Richardson Source Only
add_regression_test(cell_local_anisotropy_richardson_base physics/cell_local_anisotropy_richardson)
if(MASSCONSISTENT_BUILD_PYTHON_BINDINGS)
  add_python_regression_test(cell_local_anisotropy_richardson_python physics/cell_local_anisotropy_richardson test_cell_local_anisotropy_richardson.py)
endif()

# Cell-Local Anisotropy - Froude Source Only
add_regression_test(cell_local_anisotropy_froude_base physics/cell_local_anisotropy_froude)
if(MASSCONSISTENT_BUILD_PYTHON_BINDINGS)
  add_python_regression_test(cell_local_anisotropy_froude_python physics/cell_local_anisotropy_froude test_cell_local_anisotropy_froude.py)
endif()

# ===========================================================================
# Analytical Wake & Turbulence Extensions (TurbOPark, Jimenez, Crespo-Hernandez)
# ===========================================================================
if(MASSCONSISTENT_BUILD_PYTHON_BINDINGS)
  add_python_regression_test(analytical_wake_extensions wakes/analytical_wake_extensions test_phase2_analytical_wake_extensions.py)
  add_python_regression_test(analytical_wake_new_features wakes/analytical_wake_new_features test_analytical_wake_new_features.py)
  add_python_regression_test(aep_dispersion wakes/aep_dispersion test_aep_dispersion.py)
  add_python_regression_test(phase3_advanced_wake_models wakes/phase3_advanced_wake_models test_phase3_advanced_wake_models.py)
  add_python_regression_test(bastankhah_deflection wakes/bastankhah_deflection test_bastankhah_deflection.py)
  add_python_regression_test(polygon_shapes wakes/wake_polygon_shapes test_polygon_shapes.py)
  add_python_regression_test(polygon_huber_snyder wakes/wake_polygon_huber_snyder test_polygon_huber_snyder.py)
  add_python_regression_test(polygon_aermod_prime wakes/wake_polygon_aermod_prime test_polygon_aermod_prime.py)
  add_python_regression_test(polygon_courtyard wakes/wake_courtyard_modeling test_polygon_courtyard.py)
  add_python_regression_test(wildfire_levelset_coupling physics/wildfire_levelset_coupling test_wildfire_coupling.py)
else()
  message(STATUS "  wakes/analytical_wake_extensions: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  wakes/aep_dispersion: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  wakes/phase3_advanced_wake_models: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  wakes/wake_polygon_shapes: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  wakes/wake_polygon_huber_snyder: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  wakes/wake_polygon_aermod_prime: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  wakes/wake_courtyard_modeling: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
  message(STATUS "  physics/wildfire_levelset_coupling: skipped (requires -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON)")
endif()

# ===========================================================================
# netcdf_ingestion/ – NetCDF 3D wind parsing and ingestion regtests
# ===========================================================================
add_regression_test(netcdf_cpp_ingestion diagnostics/netcdf_cpp_ingestion)
add_python_regression_test(netcdf_ingestion_regtest diagnostics/netcdf_ingestion test_netcdf_ingestion.py)

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
message(STATUS "  terrain/flat_terrain/          : mass-consistent solver on flat terrain (dx=dy=dz=50 m, 2x2x2 grid)")
message(STATUS "  terrain/gaussian_hill/         : terrain-following wind on a Gaussian hill (dx=dy=30 m, dz=25 m)")
message(STATUS "  terrain/gaussian_hill_weno5/   : Gaussian hill with WENO-5 derivatives (dx=dy=30 m, dz=25 m)")
message(STATUS "  terrain/raws_synthetic/        : Gaussian hill with RAWS wind initialization (IDW interpolation)")
message(STATUS "  terrain/topographic_shielding/ : Central ridge with topographic barrier shielding (IDW interpolation)")
message(STATUS "  terrain/surface_data_synthetic/: Gaussian hill with surface_data initialization (HRRR-style)")
message(STATUS "  terrain/uniform_wind/          : Gaussian hill with uniform wind initialization")
message(STATUS "  terrain/powerlaw_flat/         : Power-law wind profile on flat terrain")
message(STATUS "  terrain/powerlaw_gaussian/     : Power-law wind profile on Gaussian hill")
message(STATUS "  terrain/powerlaw_building_flat/: Power-law profile with buildings on flat terrain")
message(STATUS "  terrain/powerlaw_building_terrain/: Power-law profile with buildings on terrain")
message(STATUS "  buildings/buildings_flat/        : Buildings (union of boxes) on flat terrain")
message(STATUS "  buildings/buildings_csv/         : Buildings from CSV file on flat terrain")
message(STATUS "  buildings/buildings_terrain/     : Buildings on Gaussian hill (terrain-aligned)")
message(STATUS "  wakes/wake_single_building/  : Single building wake model (Röckle formulation)")
message(STATUS "  wakes/wake_non_rectangular_sloped/: Non-rectangular and sloped building wakes")
message(STATUS "  wakes/rooftop_vortex/        : Rooftop vortex in building cavity zones")
message(STATUS "  buildings/building_oriented/     : Arbitrary building orientation effects")
message(STATUS "  buildings/building_array/        : Building array configurations")
message(STATUS "  buildings/porous_buildings/      : Building porosity model for porous structures")
message(STATUS "  buildings/eb_solid_cells/        : Embedded Boundary solid cell masking test")
message(STATUS "  diagnostics/multiheight_extraction/: Multi-height wind field extraction (10m, 50m, 100m)")
message(STATUS "  physics/wall_function_flat/    : Wall function on flat terrain")
message(STATUS "  physics/wall_function_stable/  : Wall function with stable atmospheric corrections")
message(STATUS "  physics/wall_function_unstable/: Wall function with unstable atmospheric corrections")
message(STATUS "  physics/wall_function_adaptive/: Wall function with adaptive grid resolution activation")
message(STATUS "  physics/buoyancy_stratification/: Thermal stratification with buoyancy effects")
message(STATUS "  physics/buoyancy_rhs/          : Buoyancy RHS method (source term in lambda equation)")
message(STATUS "  physics/terrain_kinematic_bc/  : Kinematic terrain-following boundary condition")
message(STATUS "  physics/scalar_transport/      : 3D scalar transport with mixing length turbulence model")
message(STATUS "  physics/combined_thermal_terrain/: Combined buoyancy and kinematic BC features")
message(STATUS "  physics/canopy_forest/         : Forest canopy with MacDonald et al. (2000) parameterization")
message(STATUS "  physics/canopy_spatially_varying/: Spatially-varying canopy height and frontal area index")
message(STATUS "  physics/canopy_exponential/    : Exponential canopy profile model")
message(STATUS "  physics/canopy_lai_profiles/   : Vertical Leaf Area Index (LAI) profiles")
message(STATUS "  physics/coriolis_meridional/   : Coriolis Meridional Beta-Effect and scaled Ekman veer")
message(STATUS "  buildings/subgrid_windbreaks/    : Sub-grid windbreaks and directional barrier drag")
message(STATUS "  physics/drag_diagnostic/       : Drag coefficient and heat flux diagnostics")
message(STATUS "  physics/stability_stable/      : Non-neutral stability (stable conditions, L > 0)")
message(STATUS "  physics/stability_unstable/    : Non-neutral stability (unstable conditions, L < 0)")
message(STATUS "  physics/ekman_veer/            : Ekman spiral wind veer correction (Coriolis effects)")
message(STATUS "  physics/alphav_height/         : Height-dependent vertical anisotropy coefficient")
message(STATUS "  physics/elevation_scaling/     : Terrain elevation effects on wind speed")
message(STATUS "  physics/time_varying/          : Time-varying wind boundary conditions")
message(STATUS "  physics/divergence_source/     : Divergence source terms for convective plumes")
message(STATUS "  physics/holtslag_stability/    : Holtslag-De Bruin stability functions")
message(STATUS "  physics/landuse_powerlaw/      : Land use-based power-law exponents")
message(STATUS "  physics/diurnal_temperature/   : Diurnal temperature profile")
message(STATUS "  physics/vegetation_roughness/  : Vegetation attenuation factor for roughness")
message(STATUS "  physics/thermodynamic_lid/     : Thermodynamic diurnal mixing height growth models")
message(STATUS "  physics/obrien_adjustment/     : O'Brien column-wise vertical velocity adjustment")
message(STATUS "  dispersion/puff_gaussian/         : Basic Gaussian puff dispersion on uniform wind")
message(STATUS "  dispersion/puff_terrain/          : Puff dispersion on terrain-following wind field")
message(STATUS "  dispersion/puff_buildings/        : Puff dispersion with building wake effects")
message(STATUS "  dispersion/puff_canopy/           : Puff dispersion with canopy drag effects")
message(STATUS "  dispersion/puff_coupled_full/     : Fully coupled puff dispersion with all features")
message(STATUS "  dispersion/puff_downwash_single_time/ : Single time step cavity trapping (AERMOD PRIME)")
message(STATUS "  dispersion/puff_downwash_multi_time/  : Multiple time steps wake recirculation and plume deformation")
message(STATUS "  dispersion/lpdm_terrain/          : Lagrangian Particle Dispersion Model on terrain")
message(STATUS "  terrain/terrain_following_steep/ : Terrain-following coordinates on steep terrain")
message(STATUS "  physics/diurnal_roughness/     : Diurnal roughness length variations z₀(t)")
message(STATUS "  physics/bl_decay/              : Exponential boundary layer wind decay above BL depth")
message(STATUS "  physics/momentum_flux/         : Momentum flux output fields (τ_x, τ_y, u*)")
message(STATUS "  physics/richardson_diagnostic/ : Richardson number boundary layer depth diagnostic")
message(STATUS "  physics/froude_scaling/        : Froude number height scaling with terrain blocking")
message(STATUS "  physics/ageostrophic_balance/  : Ageostrophic wind balance boundary conditions")
message(STATUS "")
message(STATUS "Core Surface and Wind Profile Features (Surface Flux Diagnostics and Refinement):")
message(STATUS "  physics/flux_diagnostics_feature/       : Surface flux diagnostics (SHF, LHF, momentum flux, Cd)")
message(STATUS "  physics/landuse_classification/         : NLCD land-use classification and z0 lookup")
message(STATUS "  physics/directional_bias_correction/    : Wind direction and speed bias correction")
message(STATUS "")
message(STATUS "Adaptive Parameterization Features")
message(STATUS "  wakes/wake_blending_adaptive/         : Wake deficit superposition refinement")
message(STATUS "  physics/ri_b_stability_selection/       : Conditional stability model selection")
message(STATUS "  physics/orographic_activation_thresholds/ : Orographic model activation thresholds")
message(STATUS "")
message(STATUS "Output & Integration Features")
message(STATUS "  turbulence/synthetic_turbulence_full/      : synthetic turbulence pipeline (spectral synthesis + BTS export)")
message(STATUS "  turbulence/iec61400_von_karman/            : IEC 61400-1:2019 Category B with Von Kármán spectrum")
message(STATUS "  turbulence/iec61400_kaimal/                : IEC 61400-1:2019 Category A with Kaimal spectrum")
message(STATUS "  turbulence/iec61400_category_c/            : IEC 61400-1:2019 Category C with QuadraticExponential coherence")
message(STATUS "  iec61400_categories_regtest/    : Python unit tests for IEC 61400 spectral models (20 tests)")
message(STATUS "  diagnostics/field_output_test/              : Unified field output with 21 diagnostic fields")
message(STATUS "  diagnostics/parameter_sensitivity_test/     : Parameter sensitivity sweep tool validation")
message(STATUS "  wakes/analytical_wake_extensions: Analytical turbine wake deficit, deflection, and added turbulence models")
message(STATUS "")
message(STATUS "Run tests with: ctest -L regtest  or  cmake --build build --target regtest")
