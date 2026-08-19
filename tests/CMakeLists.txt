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

# The language-resolution model: no window, no provider, no display.
add_executable(test_languageresolution
    test_languageresolution.cpp
)
target_link_libraries(test_languageresolution PRIVATE
    ${PROJECT_NAME}-lib
    Qt6::Test
)
add_test(NAME LanguageResolutionTest COMMAND test_languageresolution)

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
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME MainWindowFeaturesTest COMMAND test_mainwindow_features)

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
    ${PROJECT_NAME}-lib
    test-support
    Qt6::Test
)
add_test(NAME TranslationProviderContractTest COMMAND test_translation_provider_contract)

# When using static ONNX Runtime, also configure test executables that link
# the app library (they pull in Piper TTS transitively through it).
if(WITH_PIPER_TTS AND ONNXRuntime_USE_STATIC)
    foreach(_test_target IN ITEMS test_popupwindow_lifetime test_snippingarea_flags test_ttsprovider_isolation test_translation test_localai_provider test_settingsdialog_localai test_llmocr test_llmocr_live test_visionmodelprobe test_translationlogic test_languageresolution test_modulestatus test_statusstrip test_snippingarea_terminal test_mainwindow_status test_mainwindow_features test_appsettings_prompts test_translation_provider_contract)
        configure_onnxruntime_static(${_test_target})
        add_dependencies(${_test_target} onnxruntime_ready)
    endforeach()
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
    foreach(_test_target IN ITEMS test_helper_window test_dbus_static_init test_popupwindow_lifetime test_snippingarea_flags test_ttsprovider_isolation test_translation test_localai_provider test_settingsdialog_localai test_llmocr test_llmocr_live test_visionmodelprobe test_translationlogic test_languageresolution test_modulestatus test_statusstrip test_snippingarea_terminal test_mainwindow_status test_mainwindow_features test_appsettings_prompts test_translation_provider_contract)
        set_target_properties(${_test_target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endforeach()
    if(TARGET test_singleapplication_recovery)
        set_target_properties(test_singleapplication_recovery PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endif()
