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
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15)
    add_compile_options(-Wno-template-body)
endif()

# Ouster SDK flags
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_VIZ OFF CACHE BOOL "" FORCE)

# Submodules
add_subdirectory(thirdparty/ouster-sdk)

# Autonomy executable
add_executable(main src/main.cpp)

set_target_properties(main PROPERTIES CXX_STANDARD 23)

target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(main PRIVATE 
    ouster_client
)

