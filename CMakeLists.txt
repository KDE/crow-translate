#
# SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
# SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

cmake_minimum_required(VERSION 3.16.0)

project(crow-translate
    VERSION 3.0.0
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
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qtgstreamer-plugins-qt5, gstreamer1.0-plugins-good, gstreamer1.0-alsa, gstreamer1.0-pulseaudio, libqt5multimedia5-plugins")
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

find_package(ECM 5.83 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(QAPPLICATION_CLASS QApplication)
option(QHOTKEY_INSTALL OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(src/3rdparty/singleapplication)
add_subdirectory(src/3rdparty/qhotkey)

find_package(Qt5 REQUIRED COMPONENTS Widgets Multimedia Network LinguistTools Concurrent)
find_package(Tesseract REQUIRED)
if(UNIX)
    find_package(Qt5 REQUIRED COMPONENTS DBus)
    if(NOT APPLE)
        find_package(XCB REQUIRED COMPONENTS XCB)
        find_package(Qt5 REQUIRED COMPONENTS X11Extras)
        if(WITH_KWAYLAND)
            find_package(KF5Wayland REQUIRED COMPONENTS Client)
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
    src/languagesdialog.cpp
    src/languagesdialog.ui
    src/cli.cpp
    src/contextmenu.cpp
    src/instancepinger.cpp
    src/languagebuttonswidget.cpp
    src/languagebuttonswidget.ui
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.ui
    src/ocr/ocr.cpp
    src/ocr/screengrabbers/abstractscreengrabber.cpp
    src/ocr/screengrabbers/genericscreengrabber.cpp
    src/ocr/snippingarea.cpp
    src/onlinetranslator.cpp
    src/popupwindow.cpp
    src/popupwindow.ui
    src/screenwatcher.cpp
    src/selection.cpp
    src/settings/appsettings.cpp
    src/settings/ocrlanguageslistwidget.cpp
    src/settings/settingsdialog.cpp
    src/settings/settingsdialog.ui
    src/settings/shortcutsmodel/shortcutitem.cpp
    src/settings/shortcutsmodel/shortcutsmodel.cpp
    src/settings/shortcutsmodel/shortcutsview.cpp
    src/settings/tesseractparameterstablewidget.cpp
    src/settings/autostartmanager/abstractautostartmanager.cpp
    src/sourcetextedit.cpp
    src/speakbuttons.cpp
    src/speakbuttons.ui
    src/transitions/languagedetectedtransition.cpp
    src/transitions/ocruninitializedtransition.cpp
    src/transitions/playerstoppedtransition.cpp
    src/transitions/retranslationtransition.cpp
    src/transitions/snippingvisibletransition.cpp
    src/transitions/textemptytransition.cpp
    src/transitions/translatorabortedtransition.cpp
    src/transitions/translatorerrortransition.cpp
    src/translationedit.cpp
    src/trayicon.cpp
)

if(UNIX AND NOT APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        src/xdgdesktopportal.cpp
        src/ocr/screengrabbers/dbusscreengrabber.cpp
        src/ocr/screengrabbers/waylandgnomescreengrabber.cpp
        src/ocr/screengrabbers/waylandplasmascreengrabber.cpp
        src/ocr/screengrabbers/waylandportalscreengrabber.cpp
        src/settings/autostartmanager/unixautostartmanager.cpp
        src/settings/autostartmanager/portalautostartmanager.cpp
    )
    if(WITH_KWAYLAND)
        target_sources(${PROJECT_NAME} PRIVATE
            src/waylandhelper.cpp
        )
    endif(WITH_KWAYLAND)
elseif(APPLE)
    ecm_add_app_icon(MACOS_ICON ICONS ${APP_ICONS})

    target_sources(${PROJECT_NAME} PRIVATE
        ${MACOS_ICON}
        data/icons/app/app.qrc
        src/settings/autostartmanager/macosautostartmanager.cpp
    )
elseif(WIN32)
    ecm_add_app_icon(WINDOWS_ICON ICONS ${APP_ICONS})

    target_sources(${PROJECT_NAME} PRIVATE
        ${WINDOWS_ICON}
        data/icons/app/app.qrc
        src/settings/autostartmanager/windowsautostartmanager.cpp
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Network
    Qt5::Multimedia
    SingleApplication::SingleApplication
    QHotkey::QHotkey
    Qt5::Concurrent
    Tesseract::Tesseract
)

if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::DBus)
    if(NOT APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            XCB::XCB
            Qt5::X11Extras
        )
        if(WITH_KWAYLAND)
            target_link_libraries(${PROJECT_NAME} PRIVATE KF5::WaylandClient)
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
        if(ECM_VERSION VERSION_LESS 5.93)
            include(ECMQueryQmake)
            query_qmake(QT_BIN_DIR QT_INSTALL_BINS)
        else()
            include(ECMQueryQt)
            ecm_query_qt(QT_BIN_DIR QT_INSTALL_BINS)
        endif()
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
