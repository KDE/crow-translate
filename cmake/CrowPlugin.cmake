#
# SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
# SPDX-License-Identifier: GPL-3.0-or-later
#

# Building one of crow's backends as something the application loads rather
# than something it contains.
#
#   crow_add_plugin(mozhi
#       FAMILY translation
#       SOURCES ...          # the backend itself
#       PLUGIN_SOURCES ...   # the Q_PLUGIN_METADATA wrapper around it
#       LINK_LIBRARIES ...
#   )
#
# Each plugin is an OBJECT library plus a MODULE that links it. The object
# library is not an implementation detail worth hiding: tests drive the
# concrete provider class - test_localai_provider constructs a
# LocalAiTranslationProvider and points it at a mock server - and linking the
# objects lets them do that without any test process dlopening anything.
#
# The wrapper is kept out of that object library and compiled only into the
# module, because Q_PLUGIN_METADATA emits qt_plugin_instance() under that
# exact name in a dynamic build. Two wrappers in one binary is a duplicate
# symbol, which is what a test linking two backends would be.
#
# Modules land in the build tree under plugins/crowtranslate/<family>, which
# is exactly the layout PluginManager looks for beside the executable, so a
# freshly built crow finds freshly built backends with no install step and no
# environment variable.

set(CROW_PLUGIN_BUILD_DIR ${CMAKE_BINARY_DIR}/plugins/crowtranslate)

# Where a module cannot be one.
#
# The AppImage, the Windows installer and a portable directory all ship a
# self-contained tree, and a loadable backend in one of those is a file that
# can go missing without anything noticing until the user picks it. In that
# mode the wrappers are compiled into the application with QT_STATICPLUGIN and
# registered through Qt's static-plugin table instead - the same PluginInfo,
# the same PluginManager, no caller aware of the difference.
if(WIN32 OR WITH_PORTABLE_MODE)
    set(CROW_STATIC_PLUGINS_DEFAULT ON)
else()
    set(CROW_STATIC_PLUGINS_DEFAULT OFF)
endif()
option(CROW_STATIC_PLUGINS "Compile the backends into the application instead of loading them" ${CROW_STATIC_PLUGINS_DEFAULT})

function(crow_add_plugin PLUGIN_ID)
    cmake_parse_arguments(ARG "" "FAMILY;CLASS" "SOURCES;PLUGIN_SOURCES;LINK_LIBRARIES;INCLUDE_DIRECTORIES;COMPILE_DEFINITIONS" ${ARGN})

    if(NOT ARG_FAMILY)
        message(FATAL_ERROR "crow_add_plugin(${PLUGIN_ID}): FAMILY is required (translation, tts or ocr)")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "crow_add_plugin(${PLUGIN_ID}): SOURCES is required")
    endif()
    if(NOT ARG_PLUGIN_SOURCES)
        message(FATAL_ERROR "crow_add_plugin(${PLUGIN_ID}): PLUGIN_SOURCES is required")
    endif()
    if(NOT ARG_CLASS)
        message(FATAL_ERROR "crow_add_plugin(${PLUGIN_ID}): CLASS is required - Q_IMPORT_PLUGIN needs the wrapper's class name")
    endif()

    # The family is part of the target name because the id is only unique
    # within one: the Mozhi instance translates and speaks, and ships as two
    # backends that happen to share a name.
    set(objects crow-plugin-${ARG_FAMILY}-${PLUGIN_ID}-objects)
    set(module crow-plugin-${ARG_FAMILY}-${PLUGIN_ID})

    add_library(${objects} OBJECT ${ARG_SOURCES})
    set_target_properties(${objects} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${objects} PUBLIC Crow::Core ${ARG_LINK_LIBRARIES})
    # A plugin's headers are reachable by their bare name, from inside the
    # plugin and from the tests that drive its provider class directly.
    target_include_directories(${objects} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/${ARG_FAMILY}/${PLUGIN_ID})
    if(ARG_INCLUDE_DIRECTORIES)
        target_include_directories(${objects} PUBLIC ${ARG_INCLUDE_DIRECTORIES})
    endif()
    if(ARG_COMPILE_DEFINITIONS)
        target_compile_definitions(${objects} PUBLIC ${ARG_COMPILE_DEFINITIONS})
    endif()

    set_property(GLOBAL APPEND PROPERTY CROW_PLUGIN_OBJECT_LIBRARIES ${objects})

    if(CROW_STATIC_PLUGINS)
        # The wrapper joins the object library rather than a module of its
        # own. QT_STATICPLUGIN makes moc emit qt_static_plugin_<Class>()
        # instead of the unnamespaced entry points a dynamic plugin exports,
        # which is also what stops two wrappers in one binary colliding.
        target_compile_definitions(${objects} PUBLIC QT_STATICPLUGIN)
        target_sources(${objects} PRIVATE ${ARG_PLUGIN_SOURCES})
        set_property(GLOBAL APPEND PROPERTY CROW_PLUGIN_STATIC_CLASSES ${ARG_CLASS})
        return()
    endif()

    add_library(${module} MODULE ${ARG_PLUGIN_SOURCES})
    target_link_libraries(${module} PRIVATE ${objects})
    set_target_properties(${module} PROPERTIES
        OUTPUT_NAME crow_${PLUGIN_ID}
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY ${CROW_PLUGIN_BUILD_DIR}/${ARG_FAMILY}
        RUNTIME_OUTPUT_DIRECTORY ${CROW_PLUGIN_BUILD_DIR}/${ARG_FAMILY}
    )

    if(UNIX AND NOT APPLE)
        # A module may leave symbols undefined by default, which means a
        # backend that reaches for something only the application has - as one
        # did while this was being written - links cleanly and then fails at
        # dlopen, on a user's machine, as "backend unavailable". Make it a
        # link error instead: everything a plugin uses has to come from
        # crow-core or from a library it declares.
        target_link_options(${module} PRIVATE "LINKER:--no-undefined")

        # From wherever this ends up installed, back to libcrow-core.so.
        # Computed rather than written out because KDE_INSTALL_PLUGINDIR is
        # lib/qt6/plugins on one distribution and lib64/qt6/plugins on
        # the next.
        file(RELATIVE_PATH relative_libdir
            ${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_PLUGINDIR}/crowtranslate/${ARG_FAMILY}
            ${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_LIBDIR})
        set_target_properties(${module} PROPERTIES INSTALL_RPATH "$ORIGIN/${relative_libdir}")
        install(TARGETS ${module} LIBRARY DESTINATION ${KDE_INSTALL_PLUGINDIR}/crowtranslate/${ARG_FAMILY})
    elseif(WIN32)
        # Beside the executable's plugins directory: Windows has no rpath, and
        # crow-core.dll sits next to crow.exe where the loader will find it.
        install(TARGETS ${module} RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}/plugins/crowtranslate/${ARG_FAMILY})
    endif()

    # crow itself does not link these, so nothing would build them otherwise.
    if(TARGET ${PROJECT_NAME})
        add_dependencies(${PROJECT_NAME} ${module})
    endif()
    set_property(GLOBAL APPEND PROPERTY CROW_PLUGIN_MODULES ${module})
endfunction()

# The Q_IMPORT_PLUGIN lines for a static build, and the object libraries that
# hold what they import.
#
# Written to a file that every target linking the backends compiles, rather
# than only the executable: Q_IMPORT_PLUGIN registers through a static
# initialiser with nothing referring to it, and an initialiser inside a static
# archive that nobody references is exactly the sort of thing a linker drops.
# The tests link the backends too, and they have to see them.
function(crow_write_static_plugin_imports OUTPUT_VARIABLE)
    get_property(classes GLOBAL PROPERTY CROW_PLUGIN_STATIC_CLASSES)
    set(generated ${CMAKE_BINARY_DIR}/crow_static_plugins.cpp)

    set(contents "// Generated by crow_write_static_plugin_imports(). Do not edit.\n#include <QtPlugin>\n\n")
    foreach(class IN LISTS classes)
        string(APPEND contents "Q_IMPORT_PLUGIN(${class})\n")
    endforeach()
    file(GENERATE OUTPUT ${generated} CONTENT "${contents}")

    set(${OUTPUT_VARIABLE} ${generated} PARENT_SCOPE)
endfunction()
