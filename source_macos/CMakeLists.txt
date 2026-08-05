cmake_minimum_required(VERSION 3.24)
project(Greed VERSION 1.0.41 LANGUAGES C)

# 11.0 is the oldest macOS that runs on Apple Silicon.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# The engine, the SDL platform layer and both dependencies live in
# ../source_shared and are identical to the ones source_win64 builds.  Only the
# bundle below is macOS-specific.
include(../source_shared/cmake/deps.cmake)
include(../source_shared/cmake/engine.cmake)

# Derived from greed_cdrom/LOGO.PCX; regenerate with bundle/make_icon.py.
set(GREED_ICON "${CMAKE_CURRENT_SOURCE_DIR}/bundle/Greed.icns")
set_source_files_properties(${GREED_ICON} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")

add_executable(greed MACOSX_BUNDLE ${GREED_ICON})
greed_add_engine_target(greed)

# ---- Bundle ------------------------------------------------------------------

set_target_properties(greed PROPERTIES
    OUTPUT_NAME "Greed"
    MACOSX_BUNDLE_BUNDLE_NAME "Greed"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.redshadowsoftware.greed"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/bundle/Info.plist.in"
    # Both dependencies are static, so there is no rpath to set and no
    # Contents/Frameworks at all.
)

set(GREED_APP "$<TARGET_BUNDLE_DIR:greed>")

# Game data.  Not vendored into the repo -- taken from the shipped release
# directories that are already here.
set(GREED_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../greed_final")
set(GREED_MOVIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../greed_cdrom/MOVIES")

# Both SDL3 (zlib) and libxmp (MIT) require their notices to travel with the
# binary, so ship the licence texts from the exact trees we linked against.
add_custom_command(TARGET greed POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GREED_APP}/Contents/Resources/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl3_SOURCE_DIR}/LICENSE.txt"
            "${GREED_APP}/Contents/Resources/licenses/SDL3-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${libxmp_SOURCE_DIR}/docs/COPYING"
            "${GREED_APP}/Contents/Resources/licenses/libxmp-COPYING.txt"
    VERBATIM
)

# Copy GREED.BLO, SETUP.CFG and the soundtrack if the release data is present.
add_custom_command(TARGET greed POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DDATA_DIR=${GREED_DATA_DIR}
            -DMOVIES_DIR=${GREED_MOVIES_DIR}
            -DRESOURCES=${GREED_APP}/Contents/Resources
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_data.cmake
    VERBATIM
)

# Ad-hoc sign so Gatekeeper will run a locally built bundle.
add_custom_command(TARGET greed POST_BUILD
    COMMAND codesign --force --sign - "${GREED_APP}" || true
    VERBATIM
)
