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
# uniform_wind/ – Gaussian hill with uniform wind initialization
# ===========================================================================
add_regression_test(uniform_wind uniform_wind)

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

# ---------------------------------------------------------------------------
# Canopy model tests
# ---------------------------------------------------------------------------
# Note: If canopy tests exist, they should be added here

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
message(STATUS "  uniform_wind/          : Gaussian hill with uniform wind initialization")
message(STATUS "  buildings_flat/        : Buildings (union of boxes) on flat terrain")
message(STATUS "  buildings_csv/         : Buildings from CSV file on flat terrain")
message(STATUS "  buildings_terrain/     : Buildings on Gaussian hill (terrain-aligned)")
message(STATUS "  buildings_flat/        : Buildings (union of boxes) on flat terrain")
message(STATUS "  buildings_csv/         : Buildings from CSV file (new format)")
message(STATUS "")
message(STATUS "Run tests with: ctest -L regtest  or  cmake --build build --target regtest")
