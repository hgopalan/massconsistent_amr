# Python bindings for massconsistent_amr using pybind11
# Provides interface to control mass-consistent wind solver from Python

cmake_minimum_required(VERSION 3.20)

# Find Python3 and required components
find_package(Python3 REQUIRED COMPONENTS Interpreter Development NumPy)

message(STATUS "Python bindings enabled")
message(STATUS "  Python executable: ${Python3_EXECUTABLE}")
message(STATUS "  Python version: ${Python3_VERSION}")
message(STATUS "  Python include dirs: ${Python3_INCLUDE_DIRS}")
message(STATUS "  Python libraries: ${Python3_LIBRARIES}")

# Find or fetch pybind11
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
  message(STATUS "pybind11 not found, fetching from GitHub...")
  include(FetchContent)
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.11.1
  )
  FetchContent_MakeAvailable(pybind11)
else()
  message(STATUS "Found pybind11: ${pybind11_DIR}")
endif()

# Create Python module
pybind11_add_module(pyWindSolver MODULE
  pyWindSolver.cpp
  ${CMAKE_SOURCE_DIR}/src/wind_solver_api.cpp
  ${CMAKE_SOURCE_DIR}/src/wind_io_helpers.cpp
)

# Link with AMReX
target_link_libraries(pyWindSolver PRIVATE AMReX::amrex)

# Link with nlohmann_json if enabled
if(MASSCONSISTENT_USE_NLOHMANN_JSON)
  target_link_libraries(pyWindSolver PRIVATE nlohmann_json::nlohmann_json)
  target_compile_definitions(pyWindSolver PRIVATE MASSCONSISTENT_USE_NLOHMANN_JSON)
endif()

# Link with MPI if enabled
if(MASSCONSISTENT_ENABLE_MPI)
  target_link_libraries(pyWindSolver PRIVATE MPI::MPI_CXX)
endif()

# Add include directories for headers
target_include_directories(pyWindSolver PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

# On CUDA builds, compile bindings as CUDA code
if(MASSCONSISTENT_GPU_BACKEND STREQUAL "CUDA")
  set_source_files_properties(
    pyWindSolver.cpp
    ${CMAKE_SOURCE_DIR}/src/wind_solver_api.cpp
    ${CMAKE_SOURCE_DIR}/src/wind_io_helpers.cpp
    PROPERTIES LANGUAGE CUDA
  )
endif()

# Set output directory for the Python module
set_target_properties(pyWindSolver PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python
)

# Ensure Python3_SITELIB is defined and relative for proper wheel packaging.
# When compiling for a wheel with scikit-build-core, Python3_SITELIB may be absolute,
# so we fall back to "." to install everything directly into the root of the site-packages prefix.
if(SKBUILD OR SKBUILD_CORE_VERSION)
  if(NOT DEFINED Python3_SITELIB OR IS_ABSOLUTE "${Python3_SITELIB}")
    set(Python3_SITELIB ".")
  endif()
else()
  if(NOT DEFINED Python3_SITELIB)
    set(Python3_SITELIB "python")
  endif()
endif()

# Copy Python wrapper files to build directory
# PYTHON_WRAPPERS contains all Python modules and scripts sorted alphabetically.
# REMINDER: When adding, renaming, or removing Python files, please update this list.
set(PYTHON_WRAPPERS
  __init__.py
  aep_calculator.py
  agricultural_drone.py
  coupled_wind_fire_example.py
  datacenter_heat_source.py
  datacenter_siting.py
  example_datacenter_siting.py
  example_floris_export.py
  example_floris_extensions.py
  example_iec61400_models.py
  example_layout_optimization.py
  example_openfast_export.py
  floris_coupling.py
  floris_extensions.py
  fuga_lookup.py
  iec61400_models.py
  infrastructure_models.py
  layout_optimizer.py
  mann_box.py
  pyoptimization_export.py
  pywake_coupling.py
  turbine_io.py
  wake_models.py
  wind_field_cache.py
  wind_resource_stats.py
  wind_solver.py
)

set(PYTHON_DATA_FILES
  README.md
  chemical_database.json
)

foreach(file IN LISTS PYTHON_WRAPPERS PYTHON_DATA_FILES)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/${file}
    ${CMAKE_BINARY_DIR}/python/${file}
    COPYONLY
  )
endforeach()

# Copy phreeqc_coupling directory to build directory
file(COPY phreeqc_coupling DESTINATION ${CMAKE_BINARY_DIR}/python)

# Installation support (optional)
install(TARGETS pyWindSolver
  LIBRARY DESTINATION ${Python3_SITELIB}
)

install(FILES ${PYTHON_WRAPPERS}
  DESTINATION ${Python3_SITELIB}
)

install(FILES ${PYTHON_DATA_FILES}
  DESTINATION ${Python3_SITELIB}
)

install(DIRECTORY phreeqc_coupling
  DESTINATION ${Python3_SITELIB}
  FILES_MATCHING PATTERN "*.py"
)

message(STATUS "Python bindings configured")
message(STATUS "  Module: pyWindSolver")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}/python/")
message(STATUS "  Usage: export PYTHONPATH=\${PWD}/build/python:\$PYTHONPATH")
