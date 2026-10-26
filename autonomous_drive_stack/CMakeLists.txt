cmake_minimum_required(VERSION 3.19)

project(MilaAutonomyStack
    VERSION 1.0.0
    LANGUAGES CXX
)

# Base C++ standard for dependencies (Ouster SDK requires C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Suppress GCC 15/16's strict template body checking on spdlog
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION
   VERSION_GREATER_EQUAL 15)
    add_compile_options(-Wno-template-body)
endif()


# Submodules
add_subdirectory(thirdparty/ouster-sdk)

find_package(Eigen3 CONFIG REQUIRED)
add_library(mila_projection src/sensorFusion/Projection.cpp)
target_compile_features(mila_projection PUBLIC cxx_std_23)
set_target_properties(mila_projection PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(mila_projection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(mila_projection PUBLIC Eigen3::Eigen)

# Do not enable the Ouster SDK's separate BUILD_TESTING option.
option(MILA_BUILD_TESTS "Build Mila projection tests" OFF)
if(MILA_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_executable(mila_projection_tests tests/ProjectionTest.cpp)
    target_compile_features(mila_projection_tests PRIVATE cxx_std_23)
    set_target_properties(mila_projection_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(mila_projection_tests PRIVATE mila_projection GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(mila_projection_tests TEST_PREFIX projection.)
endif()
# rexport examples
# Autonomy executable
add_executable(main src/main.cpp)

set_target_properties(main PROPERTIES CXX_STANDARD 23)

target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
# If you need to link more libraries link them here for ousster refer to for namespaces
# https://docs.ouster.com/sdk-docs/cpp/api_cpp/namespace_ouster__sdk.html
target_link_libraries(main PRIVATE 
    ouster_client
    ouster_osf
    ouster_sensor
    ouster_pcap
    ouster_perception
)

