# cmake_minimum_required(VERSION 3.16.0)
# include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# project(freeVitals)
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mila)
# set(CMAKE_CXX_STANDARD 26) # This sets the C++ standard for the entire project
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # --- Compile-Time Information ---
# message(STATUS "--- Build Environment Info ---")
# message(STATUS "Compiler ID:      ${CMAKE_CXX_COMPILER_ID}")
# message(STATUS "Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
# message(STATUS "Compiler Path:    ${CMAKE_CXX_COMPILER}")
# message(STATUS "Target System:    ${CMAKE_SYSTEM_NAME} - ${CMAKE_SYSTEM_PROCESSOR}")
# message(STATUS "ESP-IDF Version:  ${IDF_VERSION}")
# message(STATUS "Configured C++ Standard: ${CMAKE_CXX_STANDARD}")
# message(STATUS "------------------------------")