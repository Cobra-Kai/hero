cmake_minimum_required (VERSION 2.8.0)
project (hero)

SET(CMAKE_COLOR_MAKEFILE OFF)

INCLUDE (FindPkgConfig)
# find_package for SDL will find the wrong version on my system
PKG_SEARCH_MODULE (SDL2 REQUIRED sdl2)
INCLUDE_DIRECTORIES (${SDL2_INCLUDE_DIRS})
find_package (OpenGL REQUIRED)

add_executable (hero hero.c)
TARGET_LINK_LIBRARIES (hero ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES})

