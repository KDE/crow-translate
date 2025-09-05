<!-- SPDX-FileCopyrightText: none -->
<!-- SPDX-License-Identifier: CC0-1.0 -->
# Changelog

All notable changes to this project will be documented in this file.

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
