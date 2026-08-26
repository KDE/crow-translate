#
# SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
# SPDX-License-Identifier: GPL-3.0-or-later
#

find_package(Qt6 REQUIRED COMPONENTS Test Widgets Network)

# support/ holds header-only test helpers (testisolation.h) alongside the
# compiled MockHttpServer - available to every target in this directory.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/support)

# Helper GUI process for e2e/manual selection and OCR scenarios (crow-flake's
# e2e/ runner drives this via cua-driver; it is no longer spawned in-process
# by any unit test, since those now drive text entry directly through QTest
# instead of relying on a human/automation selecting real on-screen text).
add_executable(test_helper_window
    test_helper_window.cpp
)
target_link_libraries(test_helper_window PRIVATE
    Qt6::Widgets
)

add_library(test-support STATIC
    support/mockhttpserver.h
    support/mockhttpserver.cpp
)
target_link_libraries(test-support PUBLIC
    Qt6::Network
)
target_include_directories(test-support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/support)

# Source-scan regression guard (gotcha #8) - no display, no app lib needed.
add_executable(test_dbus_static_init
    test_dbus_static_init.cpp
)
target_compile_definitions(test_dbus_static_init PRIVATE SRC_DIR="${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_dbus_static_init PRIVATE
    Qt6::Test
)
add_test(NAME DBusStaticInitTest COMMAND test_dbus_static_init)

add_executable(test_popupwindow_lifetime
    test_popupwindow_lifetime.cpp
)
target_link_libraries(test_popupwindow_lifetime PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME PopupWindowLifetimeTest COMMAND test_popupwindow_lifetime)

add_executable(test_snippingarea_flags
    test_snippingarea_flags.cpp
)
target_link_libraries(test_snippingarea_flags PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME SnippingAreaFlagsTest COMMAND test_snippingarea_flags)

add_executable(test_ttsprovider_isolation
    test_ttsprovider_isolation.cpp
)
target_link_libraries(test_ttsprovider_isolation PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
if(TARGET crow-plugin-tts-piper-objects)
    # Drives PiperTTSProvider directly - the re-entrancy it pins is in the
    # class, not in how the backend is found - so it links the objects rather
    # than loading the module.
    target_link_libraries(test_ttsprovider_isolation PRIVATE crow-plugin-tts-piper-objects)
endif()
add_test(NAME TTSProviderIsolationTest COMMAND test_ttsprovider_isolation)

if(UNIX)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    add_executable(test_singleapplication_recovery
        test_singleapplication_recovery.cpp
    )
    target_compile_definitions(test_singleapplication_recovery PRIVATE CROW_BINARY_PATH="$<TARGET_FILE:${PROJECT_NAME}>")
    target_link_libraries(test_singleapplication_recovery PRIVATE
        Qt6::Test
        Qt6::DBus
    )
    add_test(NAME SingleApplicationRecoveryTest COMMAND test_singleapplication_recovery)
endif()

# The translation view's renderer, moved out of MozhiTranslationProvider.
# Golden-output tests; no display needed.
add_executable(test_translationhtmlformatter
    test_translationhtmlformatter.cpp
)
target_link_libraries(test_translationhtmlformatter PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME TranslationHtmlFormatterTest COMMAND test_translationhtmlformatter)

# Frontend selection. Runs before any QCoreApplication exists, so it is
# plain string handling and needs no display.
add_executable(test_frontendregistry
    test_frontendregistry.cpp
)
target_link_libraries(test_frontendregistry PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME FrontendRegistryTest COMMAND test_frontendregistry)

# UserNotifier: the core -> frontend message channel that replaced the
# providers' own QMessageBox calls. Headless by design - see the file header.
add_executable(test_usernotifier
    test_usernotifier.cpp
)
target_link_libraries(test_usernotifier PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME UserNotifierTest COMMAND test_usernotifier)

# Source-scan guard: forcing the language buttons to Auto has one owner.
# No display, no app lib needed.
add_executable(test_forcedauto_owner
    test_forcedauto_owner.cpp
)
target_compile_definitions(test_forcedauto_owner PRIVATE SRC_DIR="${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_forcedauto_owner PRIVATE
    Qt6::Test
)
add_test(NAME ForcedAutoOwnerTest COMMAND test_forcedauto_owner)

# Source-scan guard: QtWidgets must stay out of the headless layers. Needs no
# app library, like test_dbus_static_init.
add_executable(test_widget_free_core
    test_widget_free_core.cpp
)
target_compile_definitions(test_widget_free_core PRIVATE SRC_DIR="${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_widget_free_core PRIVATE
    Qt6::Test
)
add_test(NAME WidgetFreeCoreTest COMMAND test_widget_free_core)

# End-to-end CLI tests. Drives the real built binary via QProcess, because
# what they pin (event-loop lifecycle, stream routing, exit codes) only
# exists in a real process - see the header comment in test_cli.cpp.
add_executable(test_cli
    test_cli.cpp
)
target_compile_definitions(test_cli PRIVATE CROW_BINARY_PATH="$<TARGET_FILE:${PROJECT_NAME}>")
if(WITH_PORTABLE_MODE)
    # How the cases give a Windows crow a configuration of their own.
    target_compile_definitions(test_cli PRIVATE CROW_PORTABLE_MODE)
endif()
add_dependencies(test_cli ${PROJECT_NAME})
target_link_libraries(test_cli PRIVATE
    test-support
    # Qt6::Gui only for QImage, to write the blank picture the --image case
    # feeds to the subprocess. Nothing here shows anything.
    Qt6::Gui
    Qt6::Test
)
add_test(NAME CliTest COMMAND test_cli)

add_executable(test_translation
    test_translation.cpp
)
target_compile_definitions(test_translation PRIVATE BUILD_TESTING)
target_link_libraries(test_translation PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME TranslationTest COMMAND test_translation)

# LocalAI provider tests (gotchas #1-5) - no display needed, but does need a
# real QCoreApplication event loop for QNetworkAccessManager/MockHttpServer.
add_executable(test_localai_provider
    test_localai_provider.cpp
)
target_link_libraries(test_localai_provider PRIVATE
    crow-plugin-translation-localai-objects
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME LocalAiProviderTest COMMAND test_localai_provider)

# Canonical auto-destination language rule - pure, headless (see gotcha list).
add_executable(test_translationlogic
    test_translationlogic.cpp
)
target_link_libraries(test_translationlogic PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME TranslationLogicTest COMMAND test_translationlogic)

# What the main window is laid out for when the screen does not say how it is
# turned (offscreen, VNC, a framebuffer) - pure, headless.
add_executable(test_screenwatcher
    test_screenwatcher.cpp
)
target_link_libraries(test_screenwatcher PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME ScreenWatcherTest COMMAND test_screenwatcher)

# The language-resolution model: no window, no provider, no display.
add_executable(test_languageresolution
    test_languageresolution.cpp
)
target_link_libraries(test_languageresolution PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME LanguageResolutionTest COMMAND test_languageresolution)

# The session that owns the providers, the language model and the status
# model. Runs guiless on purpose: that it can is the claim.
add_executable(test_translationsession
    test_translationsession.cpp
)
target_link_libraries(test_translationsession PRIVATE
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME TranslationSessionTest COMMAND test_translationsession)

# LocalAI settings dialog tests (gotchas #6/#7) - needs a display (real
# MainWindow + SettingsDialog).
add_executable(test_settingsdialog_localai
    test_settingsdialog_localai.cpp
)
target_link_libraries(test_settingsdialog_localai PRIVATE
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME SettingsDialogLocalAiTest COMMAND test_settingsdialog_localai)

# VisionModelProbe model-list wire shapes - no GUI needed, runs headless.
add_executable(test_visionmodelprobe
    test_visionmodelprobe.cpp
)
target_link_libraries(test_visionmodelprobe PRIVATE
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME VisionModelProbeTest COMMAND test_visionmodelprobe)

# LlmOcr (vision-model OCR engine) contract tests - headless OK.
add_executable(test_llmocr
    test_llmocr.cpp
)
target_link_libraries(test_llmocr PRIVATE
    crow-plugin-ocr-llm-objects
    crow-plugin-translation-localai-objects
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME LlmOcrTest COMMAND test_llmocr)

# Live LlmOcr regression against a real, locally-running Ollama + vision-OCR
# model - see test_llmocr_live.cpp for why MockHttpServer can't stand in for
# this one. Headless OK; self-skips (not fails) when Ollama/the model isn't
# available, so it is safe in the default test run on machines without it.
add_executable(test_llmocr_live
    test_llmocr_live.cpp
)
target_link_libraries(test_llmocr_live PRIVATE
    crow-plugin-ocr-llm-objects
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME LlmOcrLiveTest COMMAND test_llmocr_live)

# ModuleStatus aggregator contract - headless OK (nothing is ever shown).
add_executable(test_modulestatus
    test_modulestatus.cpp
)
target_link_libraries(test_modulestatus PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME ModuleStatusTest COMMAND test_modulestatus)

# StatusStrip view contract - headless OK: visibility is asserted through
# isHidden() on never-shown widgets, so no window exposure is needed.
add_executable(test_statusstrip
    test_statusstrip.cpp
)
target_link_libraries(test_statusstrip PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME StatusStripTest COMMAND test_statusstrip)

# SnippingArea terminal-signal regression guard (empty selection used to emit
# nothing at all) - key/mouse events on a never-shown widget, headless OK.
add_executable(test_snippingarea_terminal
    test_snippingarea_terminal.cpp
)
target_link_libraries(test_snippingarea_terminal PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME SnippingAreaTerminalTest COMMAND test_snippingarea_terminal)

# Module status through a real MainWindow (Copy backend, no network): busy
# observation during the synchronous cascade, sticky errors through
# MainWindow's own reset wiring, the pop-up strip, and the ShowStatusBar
# pick-up - needs a display.
add_executable(test_mainwindow_status
    test_mainwindow_status.cpp
)
target_compile_definitions(test_mainwindow_status PRIVATE BUILD_TESTING)
target_link_libraries(test_mainwindow_status PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME MainWindowStatusTest COMMAND test_mainwindow_status)

# Everyday MainWindow features end-to-end, offline: swap, clear, copy
# buttons, abort, paste-image OCR chain, Escape-cancel, ShowStatusBar
# settings round-trip - LocalAI translation and LLM OCR both pointed at
# MockHttpServer - needs a display.
add_executable(test_mainwindow_features
    test_mainwindow_features.cpp
)
target_compile_definitions(test_mainwindow_features PRIVATE BUILD_TESTING)
target_link_libraries(test_mainwindow_features PRIVATE
    crow-plugin-ocr-llm-objects
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME MainWindowFeaturesTest COMMAND test_mainwindow_features)

# What the two "force ... language detection" settings do to the window when a
# global action starts, with both on as installed - Mozhi pointed at
# MockHttpServer - needs a display.
add_executable(test_mainwindow_forcedauto
    test_mainwindow_forcedauto.cpp
)
target_compile_definitions(test_mainwindow_forcedauto PRIVATE BUILD_TESTING)
target_link_libraries(test_mainwindow_forcedauto PRIVATE
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME MainWindowForcedAutoTest COMMAND test_mainwindow_forcedauto)

# LocalAI prompt resolution: stored-vs-default precedence, including the
# superseded default that master still persists - headless OK.
add_executable(test_appsettings_prompts
    test_appsettings_prompts.cpp
)
target_link_libraries(test_appsettings_prompts PRIVATE
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME AppSettingsPromptsTest COMMAND test_appsettings_prompts)

# Autostart without the portal: the desktop file is copied from where the
# application is installed, not from /usr - headless OK. Unix desktops only;
# Windows and macOS have their own mechanisms.
if(UNIX AND NOT APPLE)
    add_executable(test_autostart
        test_autostart.cpp
    )
    target_link_libraries(test_autostart PRIVATE
        ${PROJECT_NAME}-lib
        test-support
        Qt6::Test
    )
    add_test(NAME AutostartTest COMMAND test_autostart)
    # What the two lists further down do for the targets that exist everywhere.
    if(WITH_PIPER_TTS AND ONNXRuntime_USE_STATIC)
        configure_onnxruntime_static(test_autostart)
        add_dependencies(test_autostart onnxruntime_ready)
    endif()
    if(NOT BUILD_TESTING)
        set_target_properties(test_autostart PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endif()

# The global shortcuts' defaults: each its own, no key shared - headless OK.
add_executable(test_appsettings_shortcuts
    test_appsettings_shortcuts.cpp
)
target_link_libraries(test_appsettings_shortcuts PRIVATE
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME AppSettingsShortcutsTest COMMAND test_appsettings_shortcuts)

# The remembered recognition region is forgotten when it is not to be
# remembered any more - headless OK.
add_executable(test_appsettings_region
    test_appsettings_region.cpp
)
target_link_libraries(test_appsettings_region PRIVATE
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME AppSettingsRegionTest COMMAND test_appsettings_region)

# Generic ATranslationProvider contract - runs the same invariants (state
# machine, MainWindow's finish()/reset() cascades, the Q_UNREACHABLE()-
# guarded Processed/NoError pairing) against every backend automatically.
# See tests/contract/providertestfixtures.h for how to add a new backend.
add_executable(test_translation_provider_contract
    contract/test_translation_provider_contract.cpp
    contract/providertestfixtures.h
)
target_include_directories(test_translation_provider_contract PRIVATE contract)
target_link_libraries(test_translation_provider_contract PRIVATE
    crow-plugin-translation-mozhi-objects
    crow-plugin-translation-localai-objects
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME TranslationProviderContractTest COMMAND test_translation_provider_contract)

# The settings page a backend gets for free. Widgets are built and inspected
# by object name, never shown - headless OK, same as test_statusstrip.
add_executable(test_pluginoptionswidget
    test_pluginoptionswidget.cpp
)
target_link_libraries(test_pluginoptionswidget PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME PluginOptionsWidgetTest COMMAND test_pluginoptionswidget)

# Test binaries live in tests/, so PluginManager's "beside the executable"
# path finds nothing. Point every test at the build tree's plugin directory,
# and make sure the modules are built before any of them runs.
#
# CROW_PLUGIN_PATH replaces the search path rather than extending it, which is
# deliberate: it means a test can also point at an empty directory and see
# exactly what an installation with no backends does.
get_property(_crow_tests DIRECTORY PROPERTY TESTS)
foreach(_crow_test IN LISTS _crow_tests)
    set_tests_properties(${_crow_test} PROPERTIES ENVIRONMENT "CROW_PLUGIN_PATH=${CMAKE_BINARY_DIR}")
endforeach()

if(CROW_STATIC_PLUGINS)
    # Each test executable needs its own copy of the Q_IMPORT_PLUGIN lines,
    # for the same reason the application does: the registration is a static
    # initialiser nobody references, and it does not survive an archive.
    get_property(_crow_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_crow_target IN LISTS _crow_targets)
        get_target_property(_crow_target_type ${_crow_target} TYPE)
        get_target_property(_crow_target_libs ${_crow_target} LINK_LIBRARIES)
        # Only the ones that link the backends. The few tests that drive the
        # built binary as a subprocess, or scan sources, have nothing to
        # import and would just fail to link.
        if(_crow_target_type STREQUAL "EXECUTABLE" AND _crow_target_libs AND "${PROJECT_NAME}-lib" IN_LIST _crow_target_libs)
            target_sources(${_crow_target} PRIVATE ${CROW_STATIC_PLUGIN_IMPORTS})
        endif()
    endforeach()
endif()

get_property(_crow_plugin_modules GLOBAL PROPERTY CROW_PLUGIN_MODULES)
get_property(_crow_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_crow_test_target IN LISTS _crow_test_targets)
    add_dependencies(${_crow_test_target} ${_crow_plugin_modules})
endforeach()

# When using static ONNX Runtime, also configure test executables that link
# the app library (they pull in Piper TTS transitively through it).
# Only one test links Piper now: the rest reach speech through the session,
# which loads a module. That is the whole point of splitting it out - a build
# with the Piper plugin still has one target paying for onnxruntime, not
# twenty-two.
if(WITH_PIPER_TTS AND ONNXRuntime_USE_STATIC)
    configure_onnxruntime_static(test_ttsprovider_isolation)
    add_dependencies(test_ttsprovider_isolation onnxruntime_ready)
endif()

# tests/ is also configured (for compile_commands.json/IDE support) whenever
# CMAKE_EXPORT_COMPILE_COMMANDS is on, independent of BUILD_TESTING - but
# test_translation (and any future test using MainWindow's
# BUILD_TESTING-gated signals) can only link successfully against a
# ${PROJECT_NAME}-lib that was itself compiled with BUILD_TESTING defined.
# Keep every test target out of the default "all" build in that
# configuration so a plain `cmake --build build` (no -DBUILD_TESTING=ON)
# still succeeds; explicit `--target <test>` or a BUILD_TESTING=ON
# configuration (see build.zsh's build-test/build-test-nopiper) still builds
# and runs them.
if(NOT BUILD_TESTING)
    foreach(_test_target IN ITEMS test_pluginoptionswidget test_helper_window test_dbus_static_init test_forcedauto_owner test_cli test_widget_free_core test_usernotifier test_translationhtmlformatter test_frontendregistry test_popupwindow_lifetime test_snippingarea_flags test_ttsprovider_isolation test_translation test_localai_provider test_settingsdialog_localai test_llmocr test_llmocr_live test_visionmodelprobe test_translationlogic test_screenwatcher test_languageresolution test_translationsession test_modulestatus test_statusstrip test_snippingarea_terminal test_mainwindow_status test_mainwindow_features test_mainwindow_forcedauto test_appsettings_prompts test_appsettings_region test_translation_provider_contract)
        set_target_properties(${_test_target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endforeach()
    if(TARGET test_singleapplication_recovery)
        set_target_properties(test_singleapplication_recovery PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endif()
