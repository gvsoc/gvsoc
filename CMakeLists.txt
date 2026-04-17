cmake_minimum_required(VERSION 3.16.3)

project(gvsoc)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set(CMAKE_C_COMPILER_LAUNCHER   ccache)
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

add_subdirectory(engine)
add_subdirectory(core)
add_subdirectory(gapy)
add_subdirectory(gvtest)
add_subdirectory(gvrun)
add_subdirectory(pulpos)
add_subdirectory(config_tree)
