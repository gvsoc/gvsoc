cmake_minimum_required(VERSION 3.16.3)

project(gvsoc)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

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
add_subdirectory(plptest)
add_subdirectory(gvrun)
add_subdirectory(pulpos)

if(IS_DIRECTORY gui)
add_subdirectory(gui)
add_subdirectory(gui-timeline)
add_subdirectory(gui-database)
endif()
