cmake_minimum_required(VERSION 3.16.3)

project(gvsoc)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set(CMAKE_C_COMPILER_LAUNCHER   ccache)
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

install(
    DIRECTORY bin
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    USE_SOURCE_PERMISSIONS
)

add_subdirectory(core)
add_subdirectory(gapy)

if(IS_DIRECTORY gui)
add_subdirectory(gui)
endif()