#
# SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
# SPDX-License-Identifier: GPL-3.0-or-later
#

# Building a Crow Translate backend that lives outside Crow Translate.
#
#   crow_add_plugin_module(shouty
#       FAMILY translation
#       SOURCES shoutyplugin.cpp shoutyprovider.cpp
#   )
#
# This is the in-tree crow_add_plugin() with the parts that are nobody else's
# business taken out - the object library the tests link, and the mode that
# compiles the backends into the application. What is left is the layout: a
# MODULE library, in the directory for its family, able to find libcrow-core
# again from wherever it was installed.
#
# The conventions matter more than the function. A backend that ignores this
# and writes the same set_target_properties() by hand works exactly as well;
# what it must not do is land somewhere PluginManager does not look, or leave
# a symbol for the application to supply.

function(crow_add_plugin_module TARGET)
    cmake_parse_arguments(ARG "" "FAMILY" "SOURCES;LINK_LIBRARIES" ${ARGN})

    if(NOT ARG_FAMILY)
        message(FATAL_ERROR "crow_add_plugin_module(${TARGET}): FAMILY is required (translation, tts or ocr)")
    endif()
    if(NOT ARG_FAMILY MATCHES "^(translation|tts|ocr)$")
        message(FATAL_ERROR "crow_add_plugin_module(${TARGET}): FAMILY must be translation, tts or ocr, not '${ARG_FAMILY}'")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "crow_add_plugin_module(${TARGET}): SOURCES is required")
    endif()

    add_library(${TARGET} MODULE ${ARG_SOURCES})
    target_link_libraries(${TARGET} PRIVATE Crow::Core ${ARG_LINK_LIBRARIES})

    # AUTOMOC on the target rather than left to the caller: a backend is a
    # Q_OBJECT with a Q_PLUGIN_METADATA in it, and without moc it builds into a
    # module that declares no plugin at all - which discovery reports as
    # "somebody else's plugin" and skips in silence.
    set_target_properties(${TARGET} PROPERTIES
        AUTOMOC ON
        # No "lib" in front. Cosmetic - discovery reads metadata, not names -
        # but it keeps an installed directory looking like the ones beside it.
        PREFIX ""
        # Where PluginManager looks beside the executable, so a plugin built
        # into a tree that also holds a crow is found without installing it.
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/crowtranslate/${ARG_FAMILY}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/crowtranslate/${ARG_FAMILY}
    )

    set(destination ${Crow_PLUGIN_INSTALL_DIR}/${ARG_FAMILY})

    if(UNIX)
        # A module may leave symbols undefined by default, so a backend that
        # reaches for something only the application has links cleanly and then
        # fails at dlopen, on somebody else's machine, as "backend
        # unavailable". Make it a link error instead.
        if(NOT APPLE)
            target_link_options(${TARGET} PRIVATE "LINKER:--no-undefined")
        endif()

        # Back to libcrow-core from wherever this ends up. Relative first, so
        # an installation that gets relocated - an AppImage, a prefix somebody
        # moved - still resolves; then the absolute directory, for a plugin
        # installed under its own prefix rather than into Crow's.
        file(RELATIVE_PATH relative_libdir ${destination} ${Crow_CORE_LIBRARY_DIR})
        if(APPLE)
            set_target_properties(${TARGET} PROPERTIES
                INSTALL_RPATH "@loader_path/${relative_libdir};${Crow_CORE_LIBRARY_DIR}")
        else()
            set_target_properties(${TARGET} PROPERTIES
                INSTALL_RPATH "$ORIGIN/${relative_libdir};${Crow_CORE_LIBRARY_DIR}")
        endif()
    endif()

    # LIBRARY on every platform, Windows included: CMake files a MODULE library
    # under LIBRARY even when the artifact is a .dll, and a RUNTIME here
    # installs nothing at all without saying so.
    install(TARGETS ${TARGET} LIBRARY DESTINATION ${destination})
endfunction()
