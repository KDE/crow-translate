#
# SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
# SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
# SPDX-FileCopyrightText: 2022 Mauritius Clemens <gitlab@janitor.chat>
# SPDX-License-Identifier: GPL-3.0-or-later
#

cmake_minimum_required(VERSION 3.16.0)

project(crow-translate
    VERSION 4.1.0
    DESCRIPTION "Application that allows to translate and speak text using Mozhi"
    HOMEPAGE_URL https://apps.kde.org/crow-translate
    LANGUAGES CXX
)

set(APPLICATION_NAME "Crow Translate")
set(APPLICATION_ID "org.kde.CrowTranslate")
set(EXECUTABLE_NAME crow)
if(UNIX)
    set(DESKTOP_FILE_BASENAME ${APPLICATION_ID})
    set(DESKTOP_FILE ${DESKTOP_FILE_BASENAME}.desktop)
    set(METAINFO_FILE ${APPLICATION_ID}.metainfo.xml)
endif()

set(CPACK_PACKAGE_VENDOR ${APPLICATION_NAME})
set(CPACK_PACKAGE_EXECUTABLES ${EXECUTABLE_NAME} ${APPLICATION_NAME})
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${APPLICATION_NAME})
set(CPACK_PACKAGE_CONTACT genaloner@gmail.com)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/GPL-3.0-or-later.txt)
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackProjectConfig.cmake)

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY >=)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libonnxruntime-dev" "libsonic-dev" "libfmt-dev" "libspdlog-dev")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE GPLv3)
set(CPACK_RPM_PACKAGE_GROUP Applications/Text)
set(CPACK_RPM_COMPRESSION_TYPE lzma)
set(CPACK_NSIS_DISPLAY_NAME ${APPLICATION_NAME})
set(CPACK_NSIS_INSTALLED_ICON_NAME ${EXECUTABLE_NAME}.exe)
set(CPACK_NSIS_EXECUTABLES_DIRECTORY .)
set(CPACK_NSIS_MUI_FINISHPAGE_RUN ${EXECUTABLE_NAME}.exe)
set(CPACK_EXTERNAL_PACKAGE_SCRIPT ${CMAKE_SOURCE_DIR}/cmake/AppImage.cmake)
set(CPACK_EXTERNAL_ENABLE_STAGING YES)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_WIN32_EXECUTABLE ON)

set(QT_DEFAULT_MAJOR_VERSION 6)
if(WIN32)
    # KDEInstallDirs takes this variable from GNUInstallDir which points to "share",
    # but it's incorrect for Windows, so set it explicitly
    set(CMAKE_INSTALL_DATAROOTDIR "bin/data")
endif()

if(WIN32)
    # Include portable mode on Windows by default
    option(WITH_PORTABLE_MODE "Enable portable functionality" ON)
else()
    option(WITH_PORTABLE_MODE "Enable portable functionality" OFF)
endif()
option(WITH_KWAYLAND "Use KWayland for better Wayland integration" ON)
option(WITH_KICONTHEMES "Use KIconThemes for proper icon theme fallback" ON)

option(WITH_PIPER_TTS "Enable Piper neural TTS provider (requires onnxruntime and espeak-ng)" ON)
option(WITH_TTS "Enable text-to-speech support (Mozhi and Qt providers, plus Piper if WITH_PIPER_TTS)" ON)
option(ESPEAKNG_USE_SYSTEM "Link against a system espeak-ng found via pkg-config instead of building the bundled submodule (requires espeak-ng built from git)" OFF)

if(ESPEAKNG_USE_SYSTEM AND NOT WITH_PIPER_TTS)
    message(FATAL_ERROR "ESPEAKNG_USE_SYSTEM requires WITH_PIPER_TTS")
endif()
if(ESPEAKNG_USE_SYSTEM AND WIN32)
    message(FATAL_ERROR "ESPEAKNG_USE_SYSTEM is only supported on non-Windows platforms")
endif()


if(CMAKE_EXPORT_COMPILE_COMMANDS AND DEFINED ENV{NIX_CFLAGS_COMPILE})
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

find_package(ECM 6.4.0 REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEClangFormat)
include(CheckSymbolExists)
include(GenerateExportHeader)
file(GLOB_RECURSE CLANG_FORMAT_SOURCES src/*.cpp src/*.h tests/*.cpp tests/*.h)
list(FILTER CLANG_FORMAT_SOURCES EXCLUDE REGEX ".*/src/3rdparty/.*" )
list(FILTER CLANG_FORMAT_SOURCES EXCLUDE REGEX ".*${PROJECT_NAME}_autogen.*" )
kde_clang_format(${CLANG_FORMAT_SOURCES})
set(QAPPLICATION_CLASS QApplication)
option(QHOTKEY_INSTALL OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(src/3rdparty/singleapplication)
add_subdirectory(src/3rdparty/qhotkey)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Network LinguistTools Concurrent StateMachine TextToSpeech)

find_package(Tesseract REQUIRED)
if(WITH_KICONTHEMES)
    find_package(KF6 REQUIRED COMPONENTS IconThemes)
endif()
if(UNIX)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    if(NOT APPLE)
        find_package(XCB REQUIRED COMPONENTS XCB)
        if(WITH_KWAYLAND)
            find_package(KWayland REQUIRED COMPONENTS Client)
        endif()
    endif()
endif()

include(KDEInstallDirs)
include(CPack)
include(ECMPoQmTools)
if(UNIX)
    include(ECMInstallIcons)
endif()
if(WIN32 OR APPLE)
    include(ECMAddAppIcon)
endif()

set(APP_ICONS
    data/icons/app/128-apps-${APPLICATION_ID}.png
    data/icons/app/16-apps-${APPLICATION_ID}.png
    data/icons/app/22-apps-${APPLICATION_ID}.png
    data/icons/app/256-apps-${APPLICATION_ID}.png
    data/icons/app/32-apps-${APPLICATION_ID}.png
    data/icons/app/48-apps-${APPLICATION_ID}.png
    data/icons/app/512-apps-${APPLICATION_ID}.png
    data/icons/app/64-apps-${APPLICATION_ID}.png
)

if(WIN32)
    # Those sizes needed only for Windows, for details see:
    # https://learn.microsoft.com/en-us/microsoft-edge/progressive-web-apps-chromium/how-to/icon-theme-color
    list(APPEND APP_ICONS
        data/icons/app/44-apps-${APPLICATION_ID}.png
        data/icons/app/150-apps-${APPLICATION_ID}.png
        data/icons/app/310-apps-${APPLICATION_ID}.png
    )
endif()

file(GLOB LOCALE_DIRS RELATIVE ${CMAKE_SOURCE_DIR}/poqm poqm/*)
set(LOCALES "")
foreach(LOCALE ${LOCALE_DIRS})
    set(LOCALES "${LOCALES} QStringLiteral(\"${LOCALE}\"), ")
endforeach()

configure_file(src/cmake.h.in cmake.h)

# The frontend-agnostic core, as a shared library.
#
# Everything below is code a translation, TTS or OCR backend needs in order to
# be one: the three abstract bases, the value types they speak in, and the
# handful of helpers they share. It was part of ${PROJECT_NAME}-lib, which is
# a STATIC archive - and a static archive cannot be the common ground for
# loadable plugins. Each plugin linking it would get its own copy of
# UserNotifier's singleton and of Language's custom-language registry, and two
# copies of a base class's type information make qobject_cast across the
# module boundary fail.
#
# So it is shared, and the application links it exactly as a plugin does.
# Nothing here may depend on ${PROJECT_NAME}-lib in return: a backend must not
# need the window, the settings dialog or the frontends in order to build.
#
# Deliberately no Qt6::Widgets. tests/test_widget_free_core.cpp scans these
# sources for widget includes; this target is the same rule enforced by the
# linker.
add_library(crow-core SHARED
    src/language.h
    src/language.cpp
    src/provideroptions.h
    src/provideroptions.cpp
    src/core/builtinplugins.h
    src/core/builtinplugins.cpp
    src/core/pluginmanager.h
    src/core/pluginmanager.cpp
    src/core/settingsstore.h
    src/core/settingsstore.cpp
    src/core/translationresult.h
    src/core/usernotifier.h
    src/core/usernotifier.cpp
    src/llm/knownendpoints.h
    src/llm/knownendpoints.cpp
    src/llm/openaiendpoint.h
    src/llm/openaiendpoint.cpp
    src/onlinetranslator.h
    src/onlinetranslator.cpp
    src/playlistplayer.h
    src/playlistplayer.cpp
    src/ocr/aocrprovider.h
    src/plugininterfaces/iproviderplugin.h
    src/plugininterfaces/iocrplugin.h
    src/plugininterfaces/itranslationplugin.h
    src/plugininterfaces/ittsplugin.h
    src/plugininterfaces/provideroptionspec.h
    src/translator/atranslationprovider.h
    src/translator/atranslationprovider.cpp
    src/translator/copytranslationprovider.h
    src/translator/copytranslationprovider.cpp
    src/tts/attsprovider.h
    src/tts/attsprovider.cpp
    src/tts/noopttsprovider.h
    src/tts/noopttsprovider.cpp
    src/tts/voice.h
    src/tts/voice.cpp
)
add_library(Crow::Core ALIAS crow-core)

generate_export_header(crow-core
    BASE_NAME crow_core
    EXPORT_FILE_NAME crow_core_export.h
)

target_include_directories(crow-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

# Where the modules end up, twice over: as an absolute path, for an
# installation that stays where it was put, and as a path relative to the
# executable, for one that does not. An AppImage is the second kind - its
# whole tree moves - and so is a prefix somebody relocated, and neither has a
# reason to appear in Qt's library paths.
file(RELATIVE_PATH CROW_PLUGIN_DIR_FROM_BIN
    ${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_BINDIR}
    ${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_PLUGINDIR})
target_compile_definitions(crow-core PRIVATE
    CROW_PLUGIN_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_PLUGINDIR}"
    CROW_PLUGIN_DIR_FROM_BIN="${CROW_PLUGIN_DIR_FROM_BIN}"
)

target_link_libraries(crow-core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Multimedia
    Qt6::StateMachine
    Qt6::TextToSpeech
)

if(MSVC)
    # C4251: an exported class with a member whose type is a template over one
    # of our own structs - TranslationResult's QVector<TranslationOptions>, and
    # OnlineTranslator's copies of the same. The warning is about a consumer
    # built with a different STL, which cannot happen here: the plugins and the
    # application are built together, by the same compiler, in one tree.
    target_compile_options(crow-core PUBLIC /wd4251)
endif()

set_target_properties(crow-core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    # Nothing leaves this library unless it says CROW_CORE_EXPORT. Without
    # this, ELF exports everything by default and a forgotten macro is
    # invisible until someone builds on Windows, where it is a link error in
    # a job that takes twenty minutes to fail.
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(UNIX AND NOT APPLE)
    set_target_properties(crow-core PROPERTIES INSTALL_RPATH "$ORIGIN")
    install(TARGETS crow-core ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
elseif(WIN32)
    # Next to crow.exe: Windows resolves a DLL from the executable's directory
    # and there is no rpath to point anywhere else.
    install(TARGETS crow-core RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
endif()

# Create library with all application logic (for reuse by tests)
add_library(${PROJECT_NAME}-lib STATIC
    data/icons/icon-theme.qrc
    data/icons/engines/engines.qrc
    src/languagesdialog.h
    src/languagesdialog.cpp
    src/languagesdialog.ui
    src/cli.h
    src/cli.cpp
    src/contextmenu.h
    src/contextmenu.cpp
    src/instancepinger.h
    src/instancepinger.cpp
    src/instancepingerdialog.h
    src/instancepingerdialog.cpp
    src/languagebuttonswidget.h
    src/languagebuttonswidget.cpp
    src/languagebuttonswidget.ui
    src/core/translationsession.h
    src/core/translationsession.cpp
    src/gui/translationhtmlformatter.h
    src/gui/translationhtmlformatter.cpp
    src/frontend/icrowfrontend.h
    src/frontend/frontendregistry.h
    src/frontend/frontendregistry.cpp
    src/frontend/guifrontend.h
    src/frontend/guifrontend.cpp
    src/frontend/clifrontend.h
    src/frontend/clifrontend.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/mainwindow.ui
    src/modulestatus.h
    src/modulestatus.cpp
    src/ocr/screengrabbers/abstractscreengrabber.h
    src/ocr/screengrabbers/abstractscreengrabber.cpp
    src/ocr/screengrabbers/genericscreengrabber.h
    src/ocr/screengrabbers/genericscreengrabber.cpp
    src/ocr/snippingarea.h
    src/ocr/snippingarea.cpp
    src/popupwindow.h
    src/popupwindow.cpp
    src/popupwindow.ui
    src/screenwatcher.h
    src/screenwatcher.cpp
    src/statusstrip.h
    src/statusstrip.cpp
    src/statusstrip.ui
    src/selection.h
    src/selection.cpp
    src/settings/appsettings.h
    src/settings/appsettings.cpp
    src/settings/ocrlanguageslistwidget.h
    src/settings/ocrlanguageslistwidget.cpp
    src/settings/pluginoptionswidget.h
    src/settings/pluginoptionswidget.cpp
    src/settings/settingsdialog.h
    src/settings/settingsdialog.cpp
    src/settings/settingsdialog.ui
    src/settings/shortcutsmodel/shortcutitem.h
    src/settings/shortcutsmodel/shortcutitem.cpp
    src/settings/shortcutsmodel/shortcutsmodel.h
    src/settings/shortcutsmodel/shortcutsmodel.cpp
    src/settings/shortcutsmodel/shortcutsview.h
    src/settings/shortcutsmodel/shortcutsview.cpp
    src/settings/tesseractparameterstablewidget.h
    src/settings/tesseractparameterstablewidget.cpp
    src/settings/autostartmanager/abstractautostartmanager.h
    src/settings/autostartmanager/abstractautostartmanager.cpp
    src/sourcetextedit.h
    src/sourcetextedit.cpp
    src/translationedit.h
    src/translationedit.cpp
    src/trayicon.h
    src/trayicon.cpp
    src/translator/translationlogic.h
    src/translator/translationlogic.cpp
    src/translator/languageresolution.h
    src/translator/languageresolution.cpp
    src/llm/visionmodelprobe.h
    src/llm/visionmodelprobe.cpp
    src/provideroptionsmanager.h
    src/provideroptionsmanager.cpp
)

# Platform-specific sources for the library
if(UNIX AND NOT APPLE)
    target_sources(${PROJECT_NAME}-lib PRIVATE
        src/xdgdesktopportal.cpp
        src/ocr/screengrabbers/dbusscreengrabber.h
        src/ocr/screengrabbers/dbusscreengrabber.cpp
        src/ocr/screengrabbers/waylandgnomescreengrabber.h
        src/ocr/screengrabbers/waylandgnomescreengrabber.cpp
        src/ocr/screengrabbers/waylandplasmascreengrabber.h
        src/ocr/screengrabbers/waylandplasmascreengrabber.cpp
        src/ocr/screengrabbers/waylandportalscreengrabber.h
        src/ocr/screengrabbers/waylandportalscreengrabber.cpp
        src/settings/autostartmanager/unixautostartmanager.h
        src/settings/autostartmanager/unixautostartmanager.cpp
        src/settings/autostartmanager/portalautostartmanager.h
        src/settings/autostartmanager/portalautostartmanager.cpp
    )
    if(WITH_KWAYLAND)
        target_sources(${PROJECT_NAME}-lib PRIVATE
          src/waylandhelper.h
          src/waylandhelper.cpp
        )
    endif(WITH_KWAYLAND)
elseif(APPLE)
    ecm_add_app_icon(MACOS_ICON ICONS ${APP_ICONS})

    target_sources(${PROJECT_NAME}-lib PRIVATE
        ${MACOS_ICON}
        data/icons/app/app.qrc
        src/settings/autostartmanager/macosautostartmanager.h
        src/settings/autostartmanager/macosautostartmanager.cpp
    )
elseif(WIN32)
    ecm_add_app_icon(WINDOWS_ICON ICONS ${APP_ICONS})

    target_sources(${PROJECT_NAME}-lib PRIVATE
        ${WINDOWS_ICON}
        data/icons/app/app.qrc
        src/settings/autostartmanager/windowsautostartmanager.h
        src/settings/autostartmanager/windowsautostartmanager.cpp
    )
endif()

target_include_directories(${PROJECT_NAME}-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

# Create executable with just main.cpp
add_executable(${PROJECT_NAME}
    src/main.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})

if(UNIX AND NOT APPLE)
    # libcrow-core.so sits beside crow in the build tree and one directory up
    # from bin/ once installed.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN;$ORIGIN/../${KDE_INSTALL_LIBDIR}"
    )
endif()

# The backends, as modules the application loads rather than code it contains.
include(CrowPlugin)

crow_add_plugin(mozhi
    FAMILY translation
    CLASS MozhiTranslationPlugin
    SOURCES
        src/plugins/translation/mozhi/mozhitranslationprovider.h
        src/plugins/translation/mozhi/mozhitranslationprovider.cpp
    PLUGIN_SOURCES
        src/plugins/translation/mozhi/mozhiplugin.h
        src/plugins/translation/mozhi/mozhiplugin.cpp
)

crow_add_plugin(tesseract
    FAMILY ocr
    CLASS TesseractOcrPlugin
    SOURCES
        src/plugins/ocr/tesseract/tesseractocr.h
        src/plugins/ocr/tesseract/tesseractocr.cpp
    PLUGIN_SOURCES
        src/plugins/ocr/tesseract/tesseractplugin.h
        src/plugins/ocr/tesseract/tesseractplugin.cpp
    LINK_LIBRARIES Tesseract::Tesseract Qt6::Concurrent
)

crow_add_plugin(llm
    FAMILY ocr
    CLASS LlmOcrPlugin
    SOURCES
        src/plugins/ocr/llm/llmocr.h
        src/plugins/ocr/llm/llmocr.cpp
    PLUGIN_SOURCES
        src/plugins/ocr/llm/llmocrplugin.h
        src/plugins/ocr/llm/llmocrplugin.cpp
)

crow_add_plugin(localai
    FAMILY translation
    CLASS LocalAiTranslationPlugin
    SOURCES
        src/plugins/translation/localai/localaitranslationprovider.h
        src/plugins/translation/localai/localaitranslationprovider.cpp
    PLUGIN_SOURCES
        src/plugins/translation/localai/localaiplugin.h
        src/plugins/translation/localai/localaiplugin.cpp
)

# Link executable against library
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}-lib)

# ecm_add_app_icon() attaches WINDOWS_ICON.rc to the static library above.
# The MSVC linker drops resources that are compiled into a static library
# when the executable does not reference them (resources expose no symbol
# that would force the object to be pulled in), so the application icon
# never ends up in crow.exe and Windows shows a generic icon. Attach the
# .rc to the executable as well so IDI_ICON1 is always embedded.
if(WIN32 AND WINDOWS_ICON)
    target_sources(${PROJECT_NAME} PRIVATE ${WINDOWS_ICON})
endif()

# Note: ONNX Runtime configuration moved after find_package(ONNXRuntime) below

# Handle espeak-ng submodule for Piper TTS
if(WITH_TTS)
    # Still a compile definition on the application, because the window and
    # the settings dialog have a whole section that makes no sense in a build
    # with no speech at all. Which *backends* exist is a runtime question now,
    # and asked of PluginManager.
    target_compile_definitions(${PROJECT_NAME}-lib PUBLIC WITH_TTS)

    crow_add_plugin(qt
        FAMILY tts
        CLASS QtTtsPlugin
        SOURCES
            src/plugins/tts/qt/qtttsprovider.h
            src/plugins/tts/qt/qtttsprovider.cpp
        PLUGIN_SOURCES
            src/plugins/tts/qt/qtttsplugin.h
            src/plugins/tts/qt/qtttsplugin.cpp
        LINK_LIBRARIES Qt6::TextToSpeech
    )

    crow_add_plugin(mozhi
        FAMILY tts
        CLASS MozhiTtsPlugin
        SOURCES
            src/plugins/tts/mozhi/mozhittsprovider.h
            src/plugins/tts/mozhi/mozhittsprovider.cpp
        PLUGIN_SOURCES
            src/plugins/tts/mozhi/mozhittsplugin.h
            src/plugins/tts/mozhi/mozhittsplugin.cpp
    )
else()
    if(WITH_PIPER_TTS)
        message(FATAL_ERROR "WITH_PIPER_TTS requires WITH_TTS")
    endif()
endif()

if(WITH_PIPER_TTS)
    # Declared before the two espeak-ng branches below, because both of them
    # configure it. Everything Piper drags in - espeak-ng, ONNX Runtime, and
    # on a static build a link script full of archives - now lands on this
    # module instead of on the application, which is the packaging point of
    # splitting it out: onnxruntime is paid for when Piper is installed.
    crow_add_plugin(piper
        FAMILY tts
        CLASS PiperTtsPlugin
        SOURCES
            src/plugins/tts/piper/piperttsprovider.h
            src/plugins/tts/piper/piperttsprovider.cpp
        PLUGIN_SOURCES
            src/plugins/tts/piper/piperttsplugin.h
            src/plugins/tts/piper/piperttsplugin.cpp
    )
    set(PIPER_OBJECTS crow-plugin-tts-piper-objects)
    set(PIPER_MODULE crow-plugin-tts-piper)
endif()

if(WITH_PIPER_TTS AND ESPEAKNG_USE_SYSTEM)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ESPEAKNG IMPORTED_TARGET espeak-ng)
    if(NOT ESPEAKNG_FOUND)
        message(FATAL_ERROR "ESPEAKNG_USE_SYSTEM is enabled but no system espeak-ng was found via pkg-config")
    endif()

    # crow uses espeak_TextToPhonemesWithTerminator, which is only available in
    # espeak-ng built from git (https://github.com/espeak-ng/espeak-ng/pull/2127)
    # and not in the 1.52.0 release, so probe for it instead of trusting the version.
    set(CMAKE_REQUIRED_LIBRARIES ${ESPEAKNG_LINK_LIBRARIES})
    set(CMAKE_REQUIRED_INCLUDES ${ESPEAKNG_INCLUDE_DIRS})
    check_symbol_exists(espeak_TextToPhonemesWithTerminator "espeak-ng/speak_lib.h" ESPEAKNG_HAS_PHONEMES_WITH_TERMINATOR)
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_REQUIRED_INCLUDES)
    if(NOT ESPEAKNG_HAS_PHONEMES_WITH_TERMINATOR)
        message(FATAL_ERROR "The system espeak-ng at '${ESPEAKNG_LINK_LIBRARIES}' lacks espeak_TextToPhonemesWithTerminator. Crow needs an espeak-ng built from git (https://github.com/espeak-ng/espeak-ng/pull/2127); build it from source or use the bundled submodule by leaving ESPEAKNG_USE_SYSTEM off")
    endif()

    find_package(ONNXRuntime 1.27.0 REQUIRED)
    if(ONNXRuntime_USE_STATIC)
        build_onnxruntime_static()
        add_dependencies(${PIPER_OBJECTS} onnxruntime_ready)
    endif()
    target_compile_definitions(${PROJECT_NAME}-lib PUBLIC WITH_PIPER_TTS)

    if(NOT ONNXRuntime_USE_STATIC)
        target_include_directories(${PIPER_OBJECTS} PUBLIC ${ONNXRuntime_INCLUDE_DIRS})
    else()
        target_include_directories(${PIPER_OBJECTS} PUBLIC
            ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime/include
            ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime/include/onnxruntime
            ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime/include/onnxruntime/core/session
        )
    endif()

    message(STATUS "Piper TTS support enabled using system espeak-ng ${ESPEAKNG_VERSION} at ${ESPEAKNG_LINK_LIBRARIES}")
elseif(WITH_PIPER_TTS)
    # Build espeak-ng from submodule
    set(ESPEAKNG_BUILD_DIR ${CMAKE_BINARY_DIR}/espeak_ng)
    set(ESPEAKNG_INSTALL_DIR ${CMAKE_BINARY_DIR}/espeak_ng-install)

    if(WIN32)
        set(ESPEAKNG_STATIC_LIB ${ESPEAKNG_INSTALL_DIR}/lib/espeak-ng.lib)
        set(UCD_STATIC_LIB ${ESPEAKNG_BUILD_DIR}/src/espeak_ng_submodule-build/src/ucd-tools/ucd.lib)
    else()
        set(ESPEAKNG_STATIC_LIB ${ESPEAKNG_INSTALL_DIR}/lib/libespeak-ng.a)
        set(UCD_STATIC_LIB ${ESPEAKNG_BUILD_DIR}/src/espeak_ng_submodule-build/src/ucd-tools/libucd.a)
    endif()

    include(ExternalProject)
    ExternalProject_Add(espeak_ng_submodule
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/espeak-ng
        PREFIX ${ESPEAKNG_BUILD_DIR}
        PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/espeak-ng-deps.cmake
            <SOURCE_DIR>/cmake/deps.cmake
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${ESPEAKNG_INSTALL_DIR}
            -DBUILD_SHARED_LIBS:BOOL=OFF
            -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
            -DUSE_ASYNC:BOOL=OFF
            -DUSE_MBROLA:BOOL=OFF
            -DUSE_LIBSONIC:BOOL=OFF
            -DUSE_LIBPCAUDIO:BOOL=OFF
            -DUSE_KLATT:BOOL=OFF
            -DUSE_SPEECHPLAYER:BOOL=OFF
            -DEXTRA_cmn:BOOL=ON
            -DEXTRA_ru:BOOL=ON
            "-DCMAKE_C_FLAGS=-D_FILE_OFFSET_BITS=64 -I${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/espeak-ng/src/ucd-tools/src/include"
        BUILD_BYPRODUCTS
            ${ESPEAKNG_STATIC_LIB}
            ${UCD_STATIC_LIB}
        UPDATE_DISCONNECTED TRUE
    )

    # Find ONNX Runtime (tries system packages first, then static linking fallback)
    find_package(ONNXRuntime 1.27.0 REQUIRED)

    # If using static linking, build from source with telemetry disabled
    if(ONNXRuntime_USE_STATIC)
        build_onnxruntime_static()
        # Add dependency to ensure ONNX Runtime is built before the plugin
        add_dependencies(${PIPER_OBJECTS} onnxruntime_ready)
    endif()

    # Add compile definition and sources when dependencies are found
    target_compile_definitions(${PROJECT_NAME}-lib PUBLIC WITH_PIPER_TTS)

    # On Windows, tell espeak-ng we're linking statically
    if(WIN32)
        target_compile_definitions(${PIPER_OBJECTS} PUBLIC LIBESPEAK_NG_EXPORT=)
    endif()

    # Configure ONNX Runtime includes - different for dynamic vs static
    if(NOT ONNXRuntime_USE_STATIC)
        target_include_directories(${PIPER_OBJECTS} PUBLIC
            ${ONNXRuntime_INCLUDE_DIRS}
            ${ESPEAKNG_INSTALL_DIR}/include
        )
    else()
        # Static linking: the objects need includes to compile, the module
        # needs the link options.
        target_include_directories(${PIPER_OBJECTS} PUBLIC
            ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime/include
            ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime/include/onnxruntime
            ${CMAKE_BINARY_DIR}/onnxruntime-src/src/onnxruntime/include/onnxruntime/core/session
            ${ESPEAKNG_INSTALL_DIR}/include
        )
    endif()

    # Add dependency on espeak-ng build
    add_dependencies(${PIPER_OBJECTS} espeak_ng_submodule)

    # Copy/install espeak-ng data files
    if(WIN32)
        # On Windows, copy to build directory (next to binary)
        add_custom_target(copy_espeak_data
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ESPEAKNG_INSTALL_DIR}/share/espeak-ng-data
                ${CMAKE_BINARY_DIR}/espeak-ng-data
            DEPENDS espeak_ng_submodule
            COMMENT "Copying espeak-ng data files to build directory"
        )
        add_dependencies(${PROJECT_NAME} copy_espeak_data)

        # Also install espeak-ng data next to the binary on Windows
        install(DIRECTORY ${CMAKE_BINARY_DIR}/espeak-ng-data
                DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

        # Copy ONNX Runtime DLLs to bin directory on Windows
        # Check multiple locations where DLLs might end up
        file(GLOB ONNX_DLLS
            # From system ONNX Runtime
            "${CMAKE_INSTALL_PREFIX}/lib/onnxruntime*.dll"
            "${CMAKE_INSTALL_PREFIX}/bin/onnxruntime*.dll"
            # From build directories
            "${CMAKE_BINARY_DIR}/lib/onnxruntime*.dll"
            "${CMAKE_BINARY_DIR}/bin/onnxruntime*.dll"
            # From ONNX Runtime library path (if set)
            "${ONNX_LIB_DIR}/onnxruntime*.dll"
            "${ONNX_LIB_DIR}/../bin/onnxruntime*.dll"
        )

        if(ONNX_DLLS)
            install(FILES ${ONNX_DLLS} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
            message(STATUS "Will copy ONNX Runtime DLLs to bin: ${ONNX_DLLS}")
        else()
            message(WARNING "No ONNX Runtime DLLs found - application may fail to start")
        endif()
    else()
        # On other systems, install to share directory
        install(DIRECTORY ${ESPEAKNG_INSTALL_DIR}/share/espeak-ng-data
                DESTINATION ${CMAKE_INSTALL_PREFIX}/share/crow-translate)
    endif()

    message(STATUS "Piper TTS support enabled with built-in espeak-ng")
else()
    message(STATUS "Piper TTS support disabled (enable with -DWITH_PIPER_TTS=ON)")
endif()

if(CROW_STATIC_PLUGINS)
    crow_write_static_plugin_imports(CROW_STATIC_PLUGIN_IMPORTS)
    get_property(CROW_PLUGIN_OBJECT_LIBRARIES GLOBAL PROPERTY CROW_PLUGIN_OBJECT_LIBRARIES)
    target_link_libraries(${PROJECT_NAME}-lib PUBLIC ${CROW_PLUGIN_OBJECT_LIBRARIES})
    target_compile_definitions(crow-core PUBLIC CROW_STATIC_PLUGINS)

    # Compiled into every executable that needs the backends, and not into
    # the archive between them. Q_IMPORT_PLUGIN registers through a static
    # initialiser that nothing refers to, and an unreferenced object inside a
    # static archive is precisely what a linker leaves out - which it did,
    # silently, and the application came up with one backend where there
    # should have been seven.
    target_sources(${PROJECT_NAME} PRIVATE ${CROW_STATIC_PLUGIN_IMPORTS})
    message(STATUS "Backends are compiled into the application (CROW_STATIC_PLUGINS=ON)")
else()
    message(STATUS "Backends are loadable modules, installed under ${KDE_INSTALL_PLUGINDIR}/crowtranslate")
endif()

target_link_libraries(${PROJECT_NAME}-lib PUBLIC
    Crow::Core
    Qt6::Network
    Qt6::Multimedia
    SingleApplication::SingleApplication
    QHotkey::QHotkey
    Qt6::Concurrent
    Qt6::StateMachine
)

target_link_libraries(${PROJECT_NAME}-lib PUBLIC Qt6::TextToSpeech)

if(WITH_KICONTHEMES)
    target_link_libraries(${PROJECT_NAME}-lib PUBLIC KF6::IconThemes)
    target_compile_definitions(${PROJECT_NAME}-lib PUBLIC WITH_KICONTHEMES)
endif()

# Link Piper dependencies if enabled.
#
# All of this used to land on the application: espeak-ng on the archive, and
# ONNX Runtime on the executable, because static-library link options do not
# propagate and an archive is not a link. Now there is a real link at the end
# of it - the module - so both go there, and a build without the Piper plugin
# installed does not carry onnxruntime at all.
if(WITH_PIPER_TTS AND ESPEAKNG_USE_SYSTEM)
    if(NOT ONNXRuntime_USE_STATIC)
        # Dynamic linking: use system ONNX Runtime
        target_compile_definitions(${PIPER_OBJECTS} PUBLIC WITH_ONNX_RUNTIME_DYNAMIC=1)
        target_link_libraries(${PIPER_OBJECTS} PUBLIC
            ${ONNXRuntime_LIBRARIES}
            PkgConfig::ESPEAKNG
        )
    else()
        # Static linking: configure via our custom function
        configure_onnxruntime_static(${PIPER_MODULE})
        add_dependencies(${PIPER_MODULE} onnxruntime_ready)
        target_link_libraries(${PIPER_OBJECTS} PUBLIC PkgConfig::ESPEAKNG)
    endif()
elseif(WITH_PIPER_TTS)
    if(NOT ONNXRuntime_USE_STATIC)
        # Dynamic linking: use system ONNX Runtime
        target_compile_definitions(${PIPER_OBJECTS} PUBLIC WITH_ONNX_RUNTIME_DYNAMIC=1)
        target_link_libraries(${PIPER_OBJECTS} PUBLIC
            ${ONNXRuntime_LIBRARIES}
            ${ESPEAKNG_STATIC_LIB}
            ${UCD_STATIC_LIB}
        )
    else()
        # Static linking: the archives cannot go on an object library either,
        # so espeak-ng is declared here as a usage requirement and ONNX
        # Runtime's whole-archive link script goes on the module itself.
        target_link_libraries(${PIPER_OBJECTS} PUBLIC
            ${ESPEAKNG_STATIC_LIB}
            ${UCD_STATIC_LIB}
        )

        # This must come AFTER find_package(ONNXRuntime) so the function exists
        configure_onnxruntime_static(${PIPER_MODULE})
        add_dependencies(${PIPER_MODULE} onnxruntime_ready)
    endif()
endif()

if(UNIX)
    target_link_libraries(${PROJECT_NAME}-lib PUBLIC Qt6::DBus)
    if(NOT APPLE)
        target_link_libraries(${PROJECT_NAME}-lib PUBLIC
            XCB::XCB
        )
        if(WITH_KWAYLAND)
            target_link_libraries(${PROJECT_NAME}-lib PUBLIC Plasma::KWaylandClient)
            target_compile_definitions(${PROJECT_NAME}-lib PUBLIC WITH_KWAYLAND)
        endif()
    endif()
endif()

if(WITH_PORTABLE_MODE)
    # On crow-core because CrowSettings answers "which settings file" for
    # plugins as well as for the application.
    target_compile_definitions(crow-core PUBLIC WITH_PORTABLE_MODE)
endif()

option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    target_compile_definitions(${PROJECT_NAME}-lib PUBLIC BUILD_TESTING)
endif()
if(BUILD_TESTING OR CMAKE_EXPORT_COMPILE_COMMANDS)
    enable_testing()
    add_subdirectory(tests)
endif()

ecm_install_po_files_as_qm(poqm)
if(UNIX AND NOT APPLE)
    # -DQT_BIN_DIR=/path/to/qt/executables can be passed to CMake directly
    if(NOT DEFINED QT_BIN_DIR)
      include(ECMQueryQt)
      ecm_query_qt(QT_BIN_DIR QT_INSTALL_BINS)
    endif()

    configure_file(data/${DESKTOP_FILE}.in ${CMAKE_CURRENT_BINARY_DIR}/data/${DESKTOP_FILE} @ONLY)

    install(TARGETS ${PROJECT_NAME} ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/data/${DESKTOP_FILE} DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES data/${METAINFO_FILE} DESTINATION ${KDE_INSTALL_METAINFODIR})

    ecm_install_icons(ICONS
        ${APP_ICONS}
        data/icons/app/sc-apps-${APPLICATION_ID}.svg
        data/icons/app/16-status-${APPLICATION_ID}-tray-dark.png
        data/icons/app/16-status-${APPLICATION_ID}-tray-light.png
        data/icons/app/22-status-${APPLICATION_ID}-tray-dark.png
        data/icons/app/22-status-${APPLICATION_ID}-tray-light.png
        data/icons/app/24-status-${APPLICATION_ID}-tray-dark.png
        data/icons/app/24-status-${APPLICATION_ID}-tray-light.png
        data/icons/app/sc-status-${APPLICATION_ID}-tray-dark.svg
        data/icons/app/sc-status-${APPLICATION_ID}-tray-light.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}
    )
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME ${APPLICATION_NAME}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_GUI_IDENTIFIER ${APPLICATION_ID}
        MACOSX_BUNDLE_INFO_STRING ${PROJECT_DESCRIPTION}
    )
elseif(WIN32)
    install(TARGETS ${PROJECT_NAME} ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
endif()
