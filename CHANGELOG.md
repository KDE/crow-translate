<!-- SPDX-FileCopyrightText: none -->
<!-- SPDX-License-Identifier: CC0-1.0 -->
# Changelog

All notable changes to this project will be documented in this file.

## [4.1.0] - 2026-08-18

### New Features

- **LocalAI Translation Backend**: Translate through any OpenAI-compatible endpoint (Ollama, LM Studio, FastFlowLM, Anthropic or a custom server), with model discovery and configurable prompts
- **Vision-Model OCR**: A second OCR engine that transcribes images with a vision model, selectable alongside Tesseract in settings
- **Modular OCR**: Screen capture, image drop/paste and the D-Bus interface all route through whichever OCR engine is active
- **Open an Image for OCR**: Drop, paste or open an image file directly, with a preview overlay; Escape cancels
- **Module Status Strip**: A strip at the bottom of the main and popup windows showing what is currently running - capture, recognition, detection, translation or speech - and what failed
- **LibreTranslate API Key**: Mozhi instances can take an API key, with a toggle between Mozhi-proxied and direct LibreTranslate API shapes
- **Report Bug Action**: A tray-menu entry that opens bugs.kde.org with the version and OS pre-filled
- **Build Without TTS**: A `WITH_TTS` CMake option that compiles out the text-to-speech engines and their UI
- **Translate Button in the Popup Window**: The popup window gained the main window's translate action

### Fixes

- Fixed the capture area on Windows with multiple monitors at per-monitor DPI scaling
- Fixed the missing application icon on Windows and macOS
- Icons now load through KIconThemes with a theme fallback
- Fixed recovery after an unclean shutdown, which could leave the application unable to start
- Fixed the "translate selection" shortcut on Wayland
- Fixed Hebrew on the Yandex and DuckDuckGo engines
- Fixed the autostart desktop file name
- Translation failures are now shown instead of being swallowed, and their messages are translatable
- Fixed various typos

### Accessibility and Localization

- Icon-only buttons throughout the main window, popup window, settings dialog and languages dialog now carry accessible names
- Label and control pairs are associated for screen readers and keyboard navigation
- User-facing error messages from every translation backend are now translatable

### Development

- Added a regression test suite covering the translation providers, OCR engines, settings, status model and window behaviour
- ONNX Runtime is found from craft installs, builds against 1.27, and tolerates newer GCC

## [4.0.0] - 2025-09-05

### Major Changes

- **Qt6 Migration**: Complete port from Qt5 to Qt6 with improved performance and compatibility
- **Provider System**: Complete architectural rewrite with modular translation and TTS providers
- **Language Class**: Replaced `OnlineTranslator::Language` with comprehensive `Language` class supporting custom languages
- **Neural TTS**: Added high-quality Piper neural text-to-speech with ONNX Runtime integration

### New Features

#### Translation & TTS Providers
- **Provider Abstraction**: New `ATranslationProvider` and `ATTSProvider` base classes for modular backends
- **Mozhi Translation Provider**: Enhanced Mozhi integration with improved error handling
- **Piper TTS Provider**: High-quality offline neural text-to-speech using ONNX Runtime
- **Qt TTS Provider**: System text-to-speech integration via Qt6 TextToSpeech
- **Mozhi TTS Provider**: Online TTS via Mozhi proxy

#### Language System
- **Custom Language Support**: Register and persist custom languages beyond standard locales
- **BCP47 Display Names**: Unique language labels showing full codes (e.g., "English (en-US)")

#### OCR Enhancements
- **Image Inversion**: Toggle image inversion before OCR processing for improved accuracy on dark backgrounds (configurable in settings)
- **Improved Tesseract Integration**: Better error handling and performance

#### Wayland & Desktop Integration
- **KWin ScreenShot2 API**: Enhanced multi-version support with proper DPI detection
- **Improved Wayland Compatibility**: Better screenshot support for snipping areas
- **D-Bus Service Fixes**: Resolved corruption issues by replacing static QDBusInterface objects

### Build System

- **CMake Updates**: Enhanced build configuration with Qt6 support
- **ONNX Runtime Integration**: Added FindONNXRuntime.cmake with vendored fallback support
- **espeak-ng Submodule**: Integrated espeak-ng for Piper phonemization
- **New Build Options**: `WITH_PIPER_TTS` for neural TTS support

### Dependencies

#### New Required Dependencies
- Qt6 (6.8+) with TextToSpeech module
- ONNX Runtime 1.22+ (for Piper TTS)
- espeak-ng bleeding edge (bundled as submodule for Piper phonemization)

#### Updated Dependencies
- Qt5 → Qt6 migration
- Updated KDE Frameworks integration

### Breaking Changes

- **Qt5 → Qt6**: (requires Qt6.8+)
- **Provider System**: Direct OnlineTranslator usage replaced with provider abstraction
- **Settings Format**: Some settings may need migration due to provider system changes

### Removed Features

- **Deprecated Transitions**: Removed unused state machine transition classes
- **Old TTS Integration**: Replaced direct TTS calls with provider system

---

## [3.1.0] - 2023-XX-XX

### Previous Release
- See Git history for 3.1.0 and earlier releases
