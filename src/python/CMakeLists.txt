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
)

# Link with AMReX
target_link_libraries(pyWindSolver PRIVATE AMReX::amrex)

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
    PROPERTIES LANGUAGE CUDA
  )
endif()

# Set output directory for the Python module
set_target_properties(pyWindSolver PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python
)

# Copy Python wrapper files to build directory
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/wind_solver.py
  ${CMAKE_BINARY_DIR}/python/wind_solver.py
  COPYONLY
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py
  ${CMAKE_BINARY_DIR}/python/__init__.py
  COPYONLY
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  ${CMAKE_BINARY_DIR}/python/README.md
  COPYONLY
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/mann_box.py
  ${CMAKE_BINARY_DIR}/python/mann_box.py
  COPYONLY
)

# Installation support (optional)
install(TARGETS pyWindSolver
  LIBRARY DESTINATION python
)

install(FILES
  wind_solver.py
  __init__.py
  README.md
  mann_box.py
  DESTINATION python
)

message(STATUS "Python bindings configured")
message(STATUS "  Module: pyWindSolver")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}/python/")
message(STATUS "  Usage: export PYTHONPATH=\${PWD}/build/python:\$PYTHONPATH")
