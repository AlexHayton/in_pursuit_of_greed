cmake_minimum_required(VERSION 3.24)
project(Greed VERSION 1.0.41 LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# The engine, the SDL platform layer and both dependencies live in
# ../source_shared and are identical to the ones source_macos builds.  Almost
# nothing is left over for this file: the port is portable because it was
# written against SDL3 rather than against an operating system.
include(../source_shared/cmake/deps.cmake)
include(../source_shared/cmake/engine.cmake)

add_executable(greed)
greed_add_engine_target(greed)

set_target_properties(greed PROPERTIES OUTPUT_NAME "Greed")

# Deliberately NOT WIN32_EXECUTABLE.  sys_main.c uses SDL_MAIN_HANDLED with a
# plain main(), and a console subsystem keeps printf and MS_Error visible --
# which is how essentially every bug catalogued in macos_plan.md was actually
# found.  Worth flipping to a GUI subsystem once the port is played through.

# No data is copied next to the exe: point GREED_DATA at the release
# directories instead (see README.md).  sys_files.c reads it as a ;-separated
# search list on Windows.
