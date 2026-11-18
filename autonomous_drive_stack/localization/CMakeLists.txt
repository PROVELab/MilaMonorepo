# Localization: GPS/RTK + IMU error-state EKF.
# Built as part of autonomous_drive_stack (add_subdirectory there). Its libraries
# come from the stack's vcpkg.json, like everything else in the stack.
cmake_minimum_required(VERSION 3.21)
project(localization LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)  # standalone build: optimise by default
endif()

find_package(Eigen3 CONFIG REQUIRED)
find_package(GeographicLib CONFIG REQUIRED)  # WGS84 <-> local metres, normal gravity
find_package(manif CONFIG REQUIRED)          # SO(3) rotation maths for the attitude
find_package(GTest CONFIG REQUIRED)

# Filter core. Precision is chosen by LOC_SCALAR (see include/loc/config.hpp);
# any extra arguments are more compile definitions.
function(add_loc_library name scalar)
  add_library(${name} src/eskf.cpp src/localizer.cpp src/live_localizer.cpp)
  target_compile_features(${name} PUBLIC cxx_std_23)
  target_include_directories(${name} PUBLIC include)
  target_compile_definitions(${name} PUBLIC LOC_SCALAR=${scalar} ${ARGN})
  target_link_libraries(${name} PUBLIC Eigen3::Eigen MANIF::manif ${GeographicLib_LIBRARIES})
  target_compile_options(${name} PRIVATE -Wall -Wextra)
endfunction()

add_loc_library(loc_f64 double)
add_loc_library(loc_f32 float)   # what an ESP32 (single-precision FPU) would run
# Every correction compiled in, so evaluations can compare sensors that the
# car's build (config.hpp) leaves out.
add_loc_library(loc_all double LOC_USE_MAG=1 LOC_USE_EXPECTED_ACCEL=1)

add_executable(replay tools/replay.cpp)
target_link_libraries(replay PRIVATE loc_f64)
add_executable(replay_f32 tools/replay.cpp)
target_link_libraries(replay_f32 PRIVATE loc_f32)
add_executable(replay_all tools/replay.cpp)
target_link_libraries(replay_all PRIVATE loc_all)

# Tests (GoogleTest): ctest --test-dir <build>/localization --output-on-failure
enable_testing()
include(GoogleTest)
add_executable(test_localizer tests/test_localizer.cpp)
target_link_libraries(test_localizer PRIVATE loc_f64 GTest::gtest_main)
gtest_discover_tests(test_localizer)
