############################################################
# CMakeLists.txt for wake enhancement unit tests
############################################################

cmake_minimum_required(VERSION 3.20)

# Include AMReX
include(${AMREX_HOME}/Tools/CMake/AMReX_Config.cmake)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# Add unit test executable
add_executable(test_wake_physics_unit test_wake_physics_unit.cpp)

# Link AMReX libraries
target_link_libraries(test_wake_physics_unit PUBLIC AMReX::amrex)

# Include paths
target_include_directories(test_wake_physics_unit PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${AMREX_HOME}/Src/Base
)

# Enable testing
enable_testing()
add_test(NAME WakePhysicsUnitTests COMMAND test_wake_physics_unit)

# Print build info
message(STATUS "Built test_wake_physics_unit")
message(STATUS "  AMReX: ${AMREX_HOME}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ Flags: ${CMAKE_CXX_FLAGS}")
