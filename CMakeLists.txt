cmake_minimum_required(VERSION 3.20)

set(AMREX_SUBMODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/amrex")
option(MASSCONSISTENT_USE_VENDORED_AMREX "Prefer building the vendored AMReX submodule" ON)
option(MASSCONSISTENT_ENABLE_MPI "Enable MPI parallelism" OFF)
option(MASSCONSISTENT_BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11" OFF)
set(MASSCONSISTENT_GPU_BACKEND "NONE" CACHE STRING
    "GPU backend for AMReX: NONE (CPU-only), CUDA, HIP, or SYCL")
set_property(CACHE MASSCONSISTENT_GPU_BACKEND PROPERTY STRINGS NONE CUDA HIP SYCL)

project(MassConsistentAMR LANGUAGES CXX C)

# Require C++17 (AMReX supports C++17+)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable additional language(s) required for the selected GPU backend
if(MASSCONSISTENT_GPU_BACKEND STREQUAL "CUDA")
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  message(STATUS "GPU backend: CUDA")
elseif(MASSCONSISTENT_GPU_BACKEND STREQUAL "HIP")
  enable_language(HIP)
  message(STATUS "GPU backend: HIP")
elseif(MASSCONSISTENT_GPU_BACKEND STREQUAL "SYCL")
  message(STATUS "GPU backend: SYCL (ensure icpx or DPC++ compiler is active)")
elseif(NOT MASSCONSISTENT_GPU_BACKEND STREQUAL "NONE")
  message(WARNING "Unknown GPU backend '${MASSCONSISTENT_GPU_BACKEND}'. Supported values: NONE CUDA HIP SYCL")
endif()

# Enable testing for regression tests
enable_testing()

if(MASSCONSISTENT_ENABLE_MPI)
  find_package(MPI REQUIRED)
  message(STATUS "MPI parallelism enabled (${MPI_CXX_COMPILER})")
endif()

if(MASSCONSISTENT_USE_VENDORED_AMREX AND EXISTS "${AMREX_SUBMODULE_DIR}/CMakeLists.txt")
  message(STATUS "Using vendored AMReX from ${AMREX_SUBMODULE_DIR}")

  # Always build in 3-D (the mass-consistent wind solver is 3-D only)
  set(AMReX_SPACEDIM "3" CACHE STRING "AMReX spatial dimensions" FORCE)

  if(MASSCONSISTENT_ENABLE_MPI)
    set(AMReX_MPI ON CACHE BOOL "Build AMReX with MPI" FORCE)
  else()
    set(AMReX_MPI OFF CACHE BOOL "Build AMReX without MPI" FORCE)
  endif()
  set(AMReX_OMP OFF CACHE BOOL "Build AMReX without OpenMP" FORCE)
  set(AMReX_GPU_BACKEND "${MASSCONSISTENT_GPU_BACKEND}" CACHE STRING "AMReX GPU backend" FORCE)
  set(AMReX_FORTRAN OFF CACHE BOOL "Build AMReX without Fortran" FORCE)
  set(AMReX_AMRLEVEL OFF CACHE BOOL "Build AMReX without AmrLevel support" FORCE)
  # Enable position-independent code for Python bindings
  if(MASSCONSISTENT_BUILD_PYTHON_BINDINGS)
    set(AMReX_PIC ON CACHE BOOL "Build AMReX with position-independent code" FORCE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  endif()
  # The mass-consistent solver uses MLMG (MLABecLaplacian), so linear solvers are required.
  set(AMReX_LINEAR_SOLVERS ON CACHE BOOL "Build AMReX with linear solvers (required for MLMG)" FORCE)
  set(AMReX_PARTICLES OFF CACHE BOOL "Build AMReX without particle support" FORCE)
  set(AMReX_EB OFF CACHE BOOL "Build AMReX without Embedded Boundary support" FORCE)
  set(AMReX_INSTALL OFF CACHE BOOL "Disable AMReX install rules when used as a subproject" FORCE)

  add_subdirectory("${AMREX_SUBMODULE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/external/amrex")
else()
  if(MASSCONSISTENT_USE_VENDORED_AMREX)
    message(STATUS "Vendored AMReX submodule not found at ${AMREX_SUBMODULE_DIR}; trying find_package(AMReX)")
  endif()

  find_package(AMReX QUIET CONFIG)

  if(AMReX_FOUND)
    message(STATUS "Using externally installed AMReX from ${AMReX_DIR}")
  else()
    message(FATAL_ERROR
      "AMReX was not found.\n"
      "Expected vendored submodule at: ${AMREX_SUBMODULE_DIR}\n"
      "Run `git submodule update --init --recursive` from the repository root, "
      "or configure with an installed AMReX via -DAMReX_DIR=/path/to/lib/cmake/AMReX."
    )
  endif()
endif()

add_executable(wind_solver src/wind_solver.cpp)
add_executable(puff_solver src/puff_solver.cpp)
add_library(wind_solver_api STATIC src/wind_solver_api.cpp)

# On Windows with CUDA, MSVC is the default host compiler and does not
# understand CUDA device-side keywords that appear in AMReX headers.
# Tell CMake to route the .cpp files through nvcc (LANGUAGE CUDA).
if(MASSCONSISTENT_GPU_BACKEND STREQUAL "CUDA")
  set_source_files_properties(
    src/wind_solver.cpp
    src/puff_solver.cpp
    src/wind_solver_api.cpp
    PROPERTIES LANGUAGE CUDA
  )
  
  # Workaround for CUDA 12.6 + Windows SDK 10.0.26100.0 with C++20
  # Known issue: nvcc on Windows with MSVC 19.40+ (VS 2022 17.10+) and C++20
  # causes CRT header redefinition errors. This is a confirmed CUDA/MSVC interaction bug.
  # Workaround: Use --allow-unsupported-compiler and add defines to suppress specific errors.
  # Note: This is not ideal but allows builds to proceed until NVIDIA fixes nvcc.
  if(WIN32)
    # Define common compile options and definitions for all CUDA targets on Windows
    set(_cuda_windows_compile_opts
      $<$<COMPILE_LANGUAGE:CUDA>:--allow-unsupported-compiler>
      $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/wd4005,/wd4244>
    )
    set(_cuda_windows_compile_defs
      $<$<COMPILE_LANGUAGE:CUDA>:_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH>
    )
    
    # Apply to all targets
    target_compile_options(wind_solver PRIVATE ${_cuda_windows_compile_opts})
    target_compile_options(puff_solver PRIVATE ${_cuda_windows_compile_opts})
    target_compile_options(wind_solver_api PRIVATE ${_cuda_windows_compile_opts})
    
    target_compile_definitions(wind_solver PRIVATE ${_cuda_windows_compile_defs})
    target_compile_definitions(puff_solver PRIVATE ${_cuda_windows_compile_defs})
    target_compile_definitions(wind_solver_api PRIVATE ${_cuda_windows_compile_defs})
  endif()
endif()

target_link_libraries(wind_solver PRIVATE AMReX::amrex)
target_link_libraries(puff_solver PRIVATE AMReX::amrex)
target_link_libraries(wind_solver_api PUBLIC AMReX::amrex)
target_include_directories(wind_solver_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MASSCONSISTENT_ENABLE_MPI)
  target_link_libraries(wind_solver PRIVATE MPI::MPI_CXX)
  target_link_libraries(wind_solver_api PUBLIC MPI::MPI_CXX)
endif()

# Recommended warnings
if(MSVC)
  target_compile_options(wind_solver PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/W4>
    $<$<COMPILE_LANGUAGE:CXX>:/W4>
  )
  target_compile_options(puff_solver PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/W4>
    $<$<COMPILE_LANGUAGE:CXX>:/W4>
  )
  target_compile_options(wind_solver_api PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/W4>
    $<$<COMPILE_LANGUAGE:CXX>:/W4>
  )
else()
  target_compile_options(wind_solver PRIVATE -Wall -Wextra -Wno-pedantic)
  target_compile_options(puff_solver PRIVATE -Wall -Wextra -Wno-pedantic)
  target_compile_options(wind_solver_api PRIVATE -Wall -Wextra -Wno-pedantic)
endif()

# HIP-specific compiler flags to handle namespace ambiguity between hiprand and amrex
if(MASSCONSISTENT_GPU_BACKEND STREQUAL "HIP")
  # Suppress ambiguous namespace errors that arise from hiprand and amrex both defining 'detail'
  target_compile_options(wind_solver PRIVATE $<$<COMPILE_LANGUAGE:HIP>:-Wno-ambiguous-member-template>)
  target_compile_options(puff_solver PRIVATE $<$<COMPILE_LANGUAGE:HIP>:-Wno-ambiguous-member-template>)
  target_compile_options(wind_solver_api PRIVATE $<$<COMPILE_LANGUAGE:HIP>:-Wno-ambiguous-member-template>)
endif()

# Add Python bindings subdirectory
if(MASSCONSISTENT_BUILD_PYTHON_BINDINGS)
  add_subdirectory(src/python)
endif()

# Add regression test subdirectory
add_subdirectory(regtest)

# Add documentation build target
option(MASSCONSISTENT_BUILD_DOCS "Build Sphinx documentation" OFF)
if(MASSCONSISTENT_BUILD_DOCS)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    find_program(SPHINX_EXECUTABLE
                 NAMES sphinx-build
                 DOC "Path to sphinx-build executable")

    if(SPHINX_EXECUTABLE)
      set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/docs)
      set(SPHINX_BUILD  ${CMAKE_CURRENT_BINARY_DIR}/docs)

      add_custom_target(docs
        COMMAND ${SPHINX_EXECUTABLE} -b html
                ${SPHINX_SOURCE} ${SPHINX_BUILD}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating documentation with Sphinx"
        VERBATIM)

      message(STATUS "Documentation build target 'docs' configured")
      message(STATUS "  Build with: cmake --build build --target docs")
      message(STATUS "  Output: ${SPHINX_BUILD}/index.html")
    else()
      message(WARNING "sphinx-build not found. Install with: pip install -r docs/requirements.txt")
    endif()
  else()
    message(WARNING "Python3 not found. Documentation build disabled.")
  endif()
endif()
