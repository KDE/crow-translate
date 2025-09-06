#
# SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
# SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
# SPDX-FileCopyrightText: 2022 Mauritius Clemens <gitlab@janitor.chat>
# SPDX-License-Identifier: GPL-3.0-or-later
#

cmake_minimum_required(VERSION 3.16.0)

project(crow-translate
    VERSION 4.0.0
    DESCRIPTION "Application that allows to translate and speak text using Mozhi"
    HOMEPAGE_URL https://apps.kde.org/crow-translate
    LANGUAGES CXX
)

set(APPLICATION_NAME "Crow Translate")
set(APPLICATION_ID "org.kde.CrowTranslate")
set(EXECUTABLE_NAME crow)
if(UNIX)
    set(DESKTOP_FILE ${APPLICATION_ID}.desktop)
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

option(WITH_PIPER_TTS "Enable Piper neural TTS provider (requires onnxruntime and espeak-ng)" ON)


if(CMAKE_EXPORT_COMPILE_COMMANDS AND DEFINED ENV{NIX_CFLAGS_COMPILE})
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

find_package(ECM 6.4.0 REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEClangFormat)
file(GLOB_RECURSE CLANG_FORMAT_SOURCES src/*.cpp src/*.h)
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

add_executable(${PROJECT_NAME}
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
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/mainwindow.ui
    src/ocr/ocr.h
    src/ocr/ocr.cpp
    src/ocr/screengrabbers/abstractscreengrabber.h
    src/ocr/screengrabbers/abstractscreengrabber.cpp
    src/ocr/screengrabbers/genericscreengrabber.h
    src/ocr/screengrabbers/genericscreengrabber.cpp
    src/ocr/snippingarea.h
    src/ocr/snippingarea.cpp
    src/onlinetranslator.h
    src/onlinetranslator.cpp
    src/popupwindow.h
    src/popupwindow.cpp
    src/popupwindow.ui
    src/screenwatcher.h
    src/screenwatcher.cpp
    src/selection.h
    src/selection.cpp
    src/settings/appsettings.h
    src/settings/appsettings.cpp
    src/settings/ocrlanguageslistwidget.h
    src/settings/ocrlanguageslistwidget.cpp
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
    src/playlistplayer.h
    src/playlistplayer.cpp
    src/tts/attsprovider.h
    src/tts/attsprovider.cpp
    src/tts/voice.h
    src/tts/voice.cpp
    src/translator/atranslationprovider.h
    src/translator/atranslationprovider.cpp
    src/tts/qtttsprovider.h
    src/tts/qtttsprovider.cpp
    src/tts/mozhittsprovider.h
    src/tts/mozhittsprovider.cpp
    src/tts/noopttsprovider.h
    src/tts/noopttsprovider.cpp
    src/translator/copytranslationprovider.h
    src/translator/copytranslationprovider.cpp
    src/translator/mozhitranslationprovider.h
    src/translator/mozhitranslationprovider.cpp
    src/provideroptions.h
    src/provideroptions.cpp
    src/provideroptionsmanager.h
    src/provideroptionsmanager.cpp
    src/language.h
    src/language.cpp
)

if(UNIX AND NOT APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
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
        target_sources(${PROJECT_NAME} PRIVATE
          src/waylandhelper.h
          src/waylandhelper.cpp
        )
    endif(WITH_KWAYLAND)
elseif(APPLE)
    ecm_add_app_icon(MACOS_ICON ICONS ${APP_ICONS})

    target_sources(${PROJECT_NAME} PRIVATE
        ${MACOS_ICON}
        data/icons/app/app.qrc
        src/settings/autostartmanager/macosautostartmanager.h
        src/settings/autostartmanager/macosautostartmanager.cpp
    )
elseif(WIN32)
    ecm_add_app_icon(WINDOWS_ICON ICONS ${APP_ICONS})

    target_sources(${PROJECT_NAME} PRIVATE
        ${WINDOWS_ICON}
        data/icons/app/app.qrc
        src/settings/autostartmanager/windowsautostartmanager.h
        src/settings/autostartmanager/windowsautostartmanager.cpp
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Handle espeak-ng submodule for Piper TTS  
if(WITH_PIPER_TTS)
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
    find_package(ONNXRuntime 1.22.1 REQUIRED)
    
    # If using static linking, build from source with telemetry disabled
    if(ONNXRuntime_USE_STATIC)
        build_onnxruntime_static()
        # Add dependency to ensure ONNX Runtime is built before main target
        add_dependencies(${PROJECT_NAME} onnxruntime_ready)
    endif()

    # Add compile definition and sources when dependencies are found
    target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_PIPER_TTS)
    
    # On Windows, tell espeak-ng we're linking statically
    if(WIN32)
        target_compile_definitions(${PROJECT_NAME} PRIVATE LIBESPEAK_NG_EXPORT=)
    endif()
    
    # Configure ONNX Runtime includes - different for dynamic vs static
    if(NOT ONNXRuntime_USE_STATIC)
        target_include_directories(${PROJECT_NAME} PRIVATE 
            ${ONNXRuntime_INCLUDE_DIRS}
            ${ESPEAKNG_INSTALL_DIR}/include
        )
    else()
        # Static linking configuration will set includes via configure_onnxruntime_static()
        target_include_directories(${PROJECT_NAME} PRIVATE 
            ${ESPEAKNG_INSTALL_DIR}/include
        )
    endif()
    target_sources(${PROJECT_NAME} PRIVATE
        src/tts/piperttsprovider.h
        src/tts/piperttsprovider.cpp
    )
    
    # Add dependency on espeak-ng build
    add_dependencies(${PROJECT_NAME} espeak_ng_submodule)
    
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

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Network
    Qt6::Multimedia
    SingleApplication::SingleApplication
    QHotkey::QHotkey
    Qt6::Concurrent
    Qt6::StateMachine
    Tesseract::Tesseract
    Qt6::TextToSpeech
)

# Link Piper dependencies if enabled
if(WITH_PIPER_TTS)
    if(NOT ONNXRuntime_USE_STATIC)
        # Dynamic linking: use system ONNX Runtime
        target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_ONNX_RUNTIME_DYNAMIC=1)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${ONNXRuntime_LIBRARIES}
            ${ESPEAKNG_STATIC_LIB}
            ${UCD_STATIC_LIB}
        )
        
        # Set RPATH for dynamic ONNX Runtime
        if(NOT WIN32)
            set_target_properties(${PROJECT_NAME} PROPERTIES
                INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib"
                BUILD_WITH_INSTALL_RPATH TRUE
            )
        endif()
    else()
        # Static linking: configure via our custom function
        configure_onnxruntime_static(${PROJECT_NAME})
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${ESPEAKNG_STATIC_LIB}
            ${UCD_STATIC_LIB}
        )
    endif()
endif()

if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::DBus)
    if(NOT APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            XCB::XCB
        )
        if(WITH_KWAYLAND)
            target_link_libraries(${PROJECT_NAME} PRIVATE Plasma::KWaylandClient)
            target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_KWAYLAND)
        endif()
    endif()
endif()

if(WITH_PORTABLE_MODE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_PORTABLE_MODE)
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
