# Dependencies, fetched and built from source at pinned tags.
#
# The closest thing C has to a lockfile: bump a tag here and the next configure
# rebuilds against it.  Nothing needs to be installed on the machine beyond
# CMake and a compiler.
#
# Both are linked statically, so Greed.app is a single self-contained binary
# with no Frameworks directory and no install_name/rpath handling at all.
#
#   SDL3   - zlib licence.
#   libxmp - MIT since 4.5.0.  It was LGPL-2.1-or-later up to 4.4.x, which
#            would have made static linking trigger the relinking obligation
#            of LGPL section 6; at the pinned 4.7.2 that no longer applies.
#            See docs/COPYING in the fetched tree.  If this is ever pinned
#            back below 4.5.0, switch libxmp to shared and restore the
#            Contents/Frameworks copy step in CMakeLists.txt.
#
# Both licences require their notices to be distributed with the binary; the
# bundle copies them into Contents/Resources/licenses/.

include(FetchContent)

set(GREED_SDL3_TAG   "release-3.4.14" CACHE STRING "SDL3 git tag to build against")
set(GREED_LIBXMP_TAG "libxmp-4.7.2"   CACHE STRING "libxmp git tag to build against")

# ---- C runtime --------------------------------------------------------------

# Windows only, and it must be set before FetchContent_MakeAvailable or the
# subprojects have already been configured with the default.  Static CRT so
# Greed.exe runs on a machine with no Visual C++ redistributable, which is the
# equivalent of the fully-static Greed.app on macOS.  SDL carries its own
# switch for the same thing; without it SDL builds /MD and the link fails on
# mismatched runtimes.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        CACHE STRING "" FORCE)
    set(SDL_FORCE_STATIC_VCRT ON CACHE BOOL "" FORCE)
endif()

# ---- SDL3 -------------------------------------------------------------------

set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        ${GREED_SDL3_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# ---- libxmp -----------------------------------------------------------------

set(BUILD_SHARED  OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC  ON  CACHE BOOL "" FORCE)
set(BUILD_LITE    OFF CACHE BOOL "" FORCE)
set(LIBXMP_DISABLE_DEPACKERS ON CACHE BOOL "" FORCE)  # the game ships bare .MOD/.S3M

# libxmp 4.7.2 declares cmake_minimum_required(VERSION 3.2...3.10).  CMake 4.x
# removed compatibility with anything below 3.5, so without this the subproject
# fails to configure outright.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

FetchContent_Declare(
    libxmp
    GIT_REPOSITORY https://github.com/libxmp/libxmp.git
    GIT_TAG        ${GREED_LIBXMP_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(SDL3 libxmp)
