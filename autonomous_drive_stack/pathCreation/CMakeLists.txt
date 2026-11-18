cmake_minimum_required(VERSION 3.19)

# The planner can also be built without the Ouster SDK for algorithm development.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ProvePlanner LANGUAGES CXX)
    include(CTest)
endif()

add_library(prove_planner src/planner.cpp src/grid_builder.cpp)
target_include_directories(prove_planner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(prove_planner PUBLIC cxx_std_17)

add_executable(planner_updates_example examples/planner_updates.cpp)
target_link_libraries(planner_updates_example PRIVATE prove_planner)

if(BUILD_TESTING)
    add_executable(planner_test tests/planner_test.cpp)
    target_link_libraries(planner_test PRIVATE prove_planner)
    add_test(NAME planner_test COMMAND planner_test)
    add_executable(grid_builder_test tests/grid_builder_test.cpp)
    target_link_libraries(grid_builder_test PRIVATE prove_planner)
    add_test(NAME grid_builder_test COMMAND grid_builder_test)
endif()
