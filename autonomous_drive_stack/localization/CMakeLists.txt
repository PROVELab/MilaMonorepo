cmake_minimum_required(VERSION 3.24)
project(localization LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Eigen3 REQUIRED NO_MODULE)
find_package(GeographicLib REQUIRED)

# manif: header-only Lie-group library (SO3 exp/log/Jacobians) used for attitude.
# Its own CMakeLists can't detect Eigen 5's version, so we only fetch the headers
# (SOURCE_SUBDIR points at a folder with no CMakeLists, so nothing is built).
# Pinned to a commit (tags can be moved): this is release 0.0.5.
include(FetchContent)
FetchContent_Declare(manif
  GIT_REPOSITORY https://github.com/artivis/manif.git
  GIT_TAG        c49fec8150e62821d9be7e2a3ac49db75bf7bb92  # 0.0.5
  SOURCE_SUBDIR  headers-only)
FetchContent_MakeAvailable(manif)
add_library(manif INTERFACE)
target_include_directories(manif SYSTEM INTERFACE
  ${manif_SOURCE_DIR}/include ${manif_SOURCE_DIR}/external/tl)

# Filter core. Precision is chosen by LOC_SCALAR (see include/loc/config.hpp);
# any extra arguments are more compile definitions.
function(add_loc_library name scalar)
  add_library(${name} src/eskf.cpp src/localizer.cpp src/live_localizer.cpp)
  target_include_directories(${name} PUBLIC include)
  target_compile_definitions(${name} PUBLIC LOC_SCALAR=${scalar} ${ARGN})
  target_link_libraries(${name} PUBLIC Eigen3::Eigen manif GeographicLib::GeographicLib)
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

# Tests: ctest --test-dir build --output-on-failure
# doctest is a single-header test library; only the header is fetched.
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        1da23a3e8119ec5cce4f9388e91b065e20bf06f5  # v2.4.12
  SOURCE_SUBDIR  doctest)
FetchContent_MakeAvailable(doctest)
enable_testing()
add_executable(test_localizer tests/test_localizer.cpp)
target_include_directories(test_localizer SYSTEM PRIVATE ${doctest_SOURCE_DIR})
target_link_libraries(test_localizer PRIVATE loc_f64)
add_test(NAME test_localizer COMMAND test_localizer)
