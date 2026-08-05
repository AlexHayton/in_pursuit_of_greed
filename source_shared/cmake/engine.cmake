# The engine target, shared by every port.
#
# source_macos/ and source_win64/ each include this, call
# greed_add_engine_target(), and then add only what is genuinely theirs -- the
# .app bundle on one side, nothing at all on the other.  Everything here is
# platform-neutral by construction: if a line needs an if(WIN32), it probably
# belongs in the caller instead.

set(GREED_SHARED_DIR "${CMAKE_CURRENT_LIST_DIR}/..")

function(greed_add_engine_target target)
    file(GLOB GREED_ENGINE_SRC CONFIGURE_DEPENDS "${GREED_SHARED_DIR}/src/*.c")

    set(GREED_PLATFORM_SRC
        ${GREED_SHARED_DIR}/platform/sys_main.c
        ${GREED_SHARED_DIR}/platform/sys_video.c
        ${GREED_SHARED_DIR}/platform/sys_input.c
        ${GREED_SHARED_DIR}/platform/sys_sound.c
        ${GREED_SHARED_DIR}/platform/sys_files.c
    )

    target_sources(${target} PRIVATE ${GREED_ENGINE_SRC} ${GREED_PLATFORM_SRC})

    # The UCRT supplies real conio/io/malloc/process/tchar headers, so Windows
    # takes a compat directory holding only dos.h.  Putting the POSIX set on
    # the include path there would shadow the genuine filelength() and _open().
    if(WIN32)
        set(GREED_COMPAT_DIR "${GREED_SHARED_DIR}/platform/compat/win")
    else()
        set(GREED_COMPAT_DIR "${GREED_SHARED_DIR}/platform/compat/posix")
    endif()

    target_include_directories(${target} PRIVATE
        ${GREED_SHARED_DIR}/src
        ${GREED_SHARED_DIR}/platform
        ${GREED_COMPAT_DIR}
    )

    # The 1996 sources predate C99: implicit int, implicit declarations, and
    # K&R-isms that a modern default standard rejects outright.
    set_target_properties(${target} PROPERTIES
        C_STANDARD 90
        C_EXTENSIONS ON
    )

    set(FORCE_INCLUDE "${GREED_SHARED_DIR}/platform/sys_compat.h")

    if(MSVC)
        target_compile_definitions(${target} PRIVATE
            # open(), filelength(), stricmp() and S_IREAD are all real in the
            # UCRT but deprecated in favour of underscored spellings; the
            # engine uses the old names throughout and we want them, not a
            # thousand C4996s.
            _CRT_NONSTDC_NO_WARNINGS
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
        )
        target_compile_options(${target} PRIVATE /FI${FORCE_INCLUDE})
    else()
        target_compile_options(${target} PRIVATE
            -fno-strict-aliasing
            -include ${FORCE_INCLUDE}
        )
    endif()

    # The pointer-truncation guards.  These are the hazards that actually
    # corrupt memory in a port like this, and they caught six real bugs during
    # the macOS port -- they are just as necessary on Windows, where the model
    # is LLP64 rather than LP64 but pointers are still 64-bit while int is not.
    # Never let them slide; each compiler spells them differently.
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        # Covers clang-cl too, which accepts -W spellings directly.
        target_compile_options(${target} PRIVATE
            -Wno-nonportable-include-path
            -Wno-parentheses
            -Wno-deprecated-non-prototype
            -Werror=int-conversion
            -Werror=pointer-to-int-cast
            -Werror=int-to-pointer-cast
            -Werror=incompatible-pointer-types
        )
        # CMake's C_STANDARD is a no-op for the clang-cl driver, so the
        # standard has to go through explicitly -- without it the tree builds
        # as C17 and Utils.c's `static weaponlump=0`, the one implicit-int
        # declaration left in the engine, becomes a hard error.
        if(MSVC)
            target_compile_options(${target} PRIVATE
                /clang:-std=gnu90 /clang:-fno-strict-aliasing)
        endif()
    elseif(MSVC)
        target_compile_options(${target} PRIVATE
            /we4311     # pointer truncation to 'int'
            /we4312     # conversion to pointer of greater size
            /we4302     # truncation
            /we4047     # differing levels of indirection
            /we4133     # incompatible pointer types
            /we4024     # different types for formal/actual parameter
            /we4029     # declared parameter list differs
            /wd4431     # missing type specifier -- int assumed (Utils.c:515)
            /wd4996     # belt and braces alongside the _CRT_* defines above
        )
        # Do NOT set /std:c11 or /std:c17 here.  MSVC's default C mode is the
        # permissive C89-with-extensions the 1996 sources need; the newer modes
        # reject implicit int outright.
    endif()

    target_link_libraries(${target} PRIVATE SDL3::SDL3-static libxmp::xmp_static)
    target_compile_definitions(${target} PRIVATE HAVE_LIBXMP=1)
endfunction()
