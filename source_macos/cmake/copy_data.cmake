# Copies the shipped game data into the app bundle's Resources directory.
# Run as a POST_BUILD script so a missing data directory is a warning, not a
# hard build failure -- the binary is still useful with GREED_DATA pointing
# somewhere else.

if(NOT EXISTS "${DATA_DIR}")
    message(WARNING "Game data not found at ${DATA_DIR}; "
                    "Greed.app will need GREED_DATA set at runtime.")
    return()
endif()

file(MAKE_DIRECTORY "${RESOURCES}")

# GREED.BLO holds every wall, sprite, sound effect and map.
foreach(name GREED.BLO SETUP.CFG)
    if(EXISTS "${DATA_DIR}/${name}")
        file(COPY "${DATA_DIR}/${name}" DESTINATION "${RESOURCES}")
    endif()
endforeach()

# Soundtrack: 12 Scream Tracker 3 modules and 6 ProTracker ones.
file(GLOB MUSIC "${DATA_DIR}/*.S3M" "${DATA_DIR}/*.MOD")
foreach(track ${MUSIC})
    file(COPY "${track}" DESTINATION "${RESOURCES}")
endforeach()

# The FLI intro movies only shipped on the CD-ROM release.
if(EXISTS "${MOVIES_DIR}")
    file(COPY "${MOVIES_DIR}" DESTINATION "${RESOURCES}")
    message(STATUS "Bundled FLI intro movies from ${MOVIES_DIR}")
endif()

message(STATUS "Bundled game data from ${DATA_DIR}")
