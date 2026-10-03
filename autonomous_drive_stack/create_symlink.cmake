# create_symlink.cmake

# Ensure this only runs once for the top-level project
if(__CREATE_SYMLINK_INCLUDED)
    return()
endif()
set(__CREATE_SYMLINK_INCLUDED TRUE)

# Ensure CMake version supports cmake_language(DEFER) (requires 3.19+)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
    cmake_language(DEFER CALL file
        CREATE_LINK
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
        SYMBOLIC
    )
endif()
