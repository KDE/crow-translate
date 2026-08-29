#
# SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
# SPDX-License-Identifier: GPL-3.0-or-later
#

# The promise in PLUGINS.md, run rather than asserted.
#
# Installs this build into a scratch prefix, then builds each of the three
# templates in examples/plugins/ against that prefix as an ordinary CMake
# project - find_package(Crow), Crow::Core, nothing that knows where Crow's
# sources are - installs them, and drives the installed crow through each one.
#
# Two things fall out of doing it this way rather than with a unit test:
#
#   The prefix is not the one this build was configured with, so
#   CROW_PLUGIN_INSTALL_DIR is baked to the wrong absolute path and only the
#   relative-from-the-executable search can find anything. That is the
#   AppImage's situation, checked here for free.
#
#   Nothing here depends on how the backends shipped with this build were
#   linked, so running it against a CROW_STATIC_PLUGINS=ON build is the
#   Windows configuration, minus the compiler.

cmake_minimum_required(VERSION 3.16)

foreach(required IN ITEMS CROW_BUILD_DIR CROW_EXAMPLES_DIR CROW_SCRATCH_DIR CROW_BINDIR CROW_TEST_IMAGE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} was not passed to this script")
    endif()
endforeach()

set(prefix ${CROW_SCRATCH_DIR}/prefix)
set(config ${CROW_SCRATCH_DIR}/config)
set(scratch ${CROW_SCRATCH_DIR}/scratch)
# The suffix comes from the parent build. Script mode never runs the platform
# setup, so CMAKE_EXECUTABLE_SUFFIX is empty here even on Windows.
set(crow ${prefix}/${CROW_BINDIR}/crow${CROW_EXECUTABLE_SUFFIX})

# Runs one command and stops the test at the first thing that does not work,
# with its output, rather than at whatever it broke three steps later.
function(crow_run_step description)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${description} failed (${result}):\n${output}\n${errors}")
    endif()
    set(crow_step_output "${output}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE ${CROW_SCRATCH_DIR})
file(MAKE_DIRECTORY ${config} ${scratch})

crow_run_step("installing this build into a scratch prefix"
    ${CMAKE_COMMAND} --install ${CROW_BUILD_DIR} --prefix ${prefix})

if(NOT EXISTS ${crow})
    message(FATAL_ERROR "the scratch install produced no executable at ${crow}")
endif()

# The blocking pair from #738: without installed headers and a findable CMake
# package, out-of-tree compilation is not a supported thing. Named here so a
# missing one fails as itself rather than as a template that will not configure.
foreach(header
        include/CrowTranslate/plugininterfaces/itranslationplugin.h
        include/CrowTranslate/plugininterfaces/ittsplugin.h
        include/CrowTranslate/plugininterfaces/iocrplugin.h
        include/CrowTranslate/plugininterfaces/provideroptionspec.h
        include/CrowTranslate/translator/atranslationprovider.h
        include/CrowTranslate/tts/attsprovider.h
        include/CrowTranslate/ocr/aocrprovider.h
        include/CrowTranslate/crow_core_export.h)
    if(NOT EXISTS ${prefix}/${header})
        message(FATAL_ERROR "make install shipped no ${header}")
    endif()
endforeach()

foreach(template IN ITEMS translation-hello tts-hello ocr-hello)
    set(build ${scratch}/${template})
    set(configure_arguments
        -S ${CROW_EXAMPLES_DIR}/${template}
        -B ${build}
        -DCMAKE_PREFIX_PATH=${prefix}
        -DCMAKE_INSTALL_PREFIX=${prefix})
    # Same generator, build type and compiler as the build being tested
    # against. A plugin built by a different compiler is a support question,
    # not something to discover here by accident.
    if(CROW_GENERATOR)
        list(APPEND configure_arguments -G ${CROW_GENERATOR})
    endif()
    if(CROW_BUILD_TYPE)
        list(APPEND configure_arguments -DCMAKE_BUILD_TYPE=${CROW_BUILD_TYPE})
    endif()
    if(CROW_CXX_COMPILER)
        list(APPEND configure_arguments -DCMAKE_CXX_COMPILER=${CROW_CXX_COMPILER})
    endif()

    # Empty on a single-configuration generator, where passing --config at all
    # is an error rather than a no-op.
    set(config_arguments)
    if(CROW_BUILD_TYPE)
        set(config_arguments --config ${CROW_BUILD_TYPE})
    endif()

    crow_run_step("configuring ${template} against the installed prefix"
        ${CMAKE_COMMAND} ${configure_arguments})
    crow_run_step("building ${template}"
        ${CMAKE_COMMAND} --build ${build} ${config_arguments})
    crow_run_step("installing ${template}"
        ${CMAKE_COMMAND} --install ${build} ${config_arguments})
endforeach()

# A settings file of our own, and no inherited search path: CROW_PLUGIN_PATH
# *replaces* where crow looks, and every other test in this directory sets it
# to the build tree. Leaving it set would mean the installed prefix - the whole
# subject here - was never consulted.
unset(ENV{CROW_PLUGIN_PATH})
set(ENV{XDG_CONFIG_HOME} ${config})
set(ENV{QT_QPA_PLATFORM} offscreen)
set(ENV{QT_LOGGING_RULES} "*=false")
# QDir::tempPath(), which is where the speech template writes.
set(ENV{TMPDIR} ${scratch})
set(ENV{TMP} ${scratch})
set(ENV{TEMP} ${scratch})

file(MAKE_DIRECTORY ${config}/crow-translate)
file(WRITE ${config}/crow-translate/crow-translate.conf "[OCR]\nEngineId=hello\n")

# Translation: the backend runs, and the option it declared with no settings
# key of its own was read from Plugins/hello/prefix and applied.
crow_run_step("translating through the out-of-tree backend"
    ${crow} --tp hello -s en -t en -b "out-of-tree")
if(NOT crow_step_output MATCHES "\\[hello\\] out-of-tree")
    message(FATAL_ERROR "the translation template produced:\n${crow_step_output}")
endif()

# Speech: --tp copy so nothing here needs a network. The template writes what
# it was asked to say, prefixed with the voice the host picked for English.
crow_run_step("speaking through the out-of-tree backend"
    ${crow} --tp copy --tts hello -o -a -s en -t en "spoken text")
if(NOT EXISTS ${scratch}/crow-hello-tts.txt)
    message(FATAL_ERROR "the speech template was never asked to say anything")
endif()
file(READ ${scratch}/crow-hello-tts.txt spoken)
if(NOT spoken MATCHES "Ink: spoken text")
    message(FATAL_ERROR "the speech template wrote:\n${spoken}")
endif()

# Recognition: the family with no built-in fallback, so this is also the check
# that a third-party engine is the only thing --image needs.
crow_run_step("recognizing through the out-of-tree backend"
    ${crow} --image ${CROW_TEST_IMAGE} --tp copy -s en -t en -b)
if(NOT crow_step_output MATCHES "hello: 16x16")
    message(FATAL_ERROR "the recognition template produced:\n${crow_step_output}")
endif()

message(STATUS "three out-of-tree backends built against ${prefix}, installed, and run")
