<!-- SPDX-FileCopyrightText: none -->
<!-- SPDX-License-Identifier: CC0-1.0 -->

# ![Crow Translate logo](data/icons/app/48-apps-org.kde.CrowTranslate.png) Crow Translate

Application written in **C++ / Qt** that allows you to translate and speak text using [Mozhi](https://codeberg.org/aryak/mozhi).

## Content

- [Screenshots](#screenshots)
- [Features](#features)
- [Default keyboard shortcuts](#default-keyboard-shortcuts)
- [CLI commands](#cli-commands)
- [D-Bus API](#d-bus-api)
- [Global shortcuts in Wayland](#global-shortcuts-in-wayland)
- [OCR](#ocr)
- [Dependencies](#dependencies)
- [Icons](#icons)
- [Installation](#installation)
- [Building](#building)
- [Localization](#localization)

## Screenshots

**Plasma**

<p align="center">
  <img src="https://invent.kde.org/websites/product-screenshots/-/raw/master/crow-translate/main.png" alt="Main"/>
</p>

**Plasma Mobile**

<p align="center">
  <img src="https://invent.kde.org/websites/product-screenshots/-/raw/master/crow-translate/main-mobile.png"alt="Main"/>
</p>

**Windows 10**

<p align="center">
  <img src="https://invent.kde.org/websites/product-screenshots/-/raw/master/crow-translate/main-windows.png" alt="Main"/>
</p>

## Features

- [Multiple translation engines](https://codeberg.org/aryak/mozhi#supported-engines) provided by Mozhi (some instances can disable specific engines)*
- Translate and speak text from screen or selection
- Highly customizable shortcuts
- Command-line interface with rich options
- D-Bus API
- Cross-platform

:warning: *While Mozhi acts as a proxy to protect your privacy, the third-party services it uses may store and analyze the text you send. 

## Default keyboard shortcuts

You can change them in the settings. Some key sequences may not be available due to OS limitations.

Wayland does not support global shortcuts registration, but you can use [D-Bus](#d-bus-api) to bind actions in the system settings. For desktop environments that support [additional applications actions](https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#extra-actions) (KDE, for example) you will see them predefined in the system shortcut settings. You can also use them for X11 sessions, but you need to disable global shortcuts registration in the application settings to avoid conflicts.

### Global

| Key                                             | Description                        |
| ----------------------------------------------- | ---------------------------------- |
| <kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>E</kbd> | Translate selected text            |
| <kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>S</kbd> | Speak selected text                |
| <kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>F</kbd> | Speak translation of selected text |
| <kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>G</kbd> | Stop speaking                      |
| <kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>C</kbd> | Show main window                   |
| <kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>I</kbd> | Recognize text in screen area      |
| <kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>O</kbd> | Translate text in screen area      |

### In main window

| Key                                               | Description                             |
| ------------------------------------------------- | --------------------------------------- |
| <kbd>Ctrl</kbd> + <kbd>Return</kbd>               | Translate                               |
| <kbd>Ctrl</kbd> + <kbd>R</kbd>                    | Swap languages                          |
| <kbd>Ctrl</kbd> + <kbd>Q</kbd>                    | Close window                            |
| <kbd>Ctrl</kbd> + <kbd>S</kbd>                    | Speak source / pause text speaking      |
| <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>S</kbd> | Speak translation / pause text speaking |
| <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>C</kbd> | Copy translation to clipboard           |

## CLI commands

The program also has a console interface.

**Usage:** `crow [options] text`

| Option                     | Description                                                                                                                            |
| -------------------------- | -------------------------------------------------------------------------------------------------------------------                    |
| `-h, --help`               | Display help                                                                                                                           |
| `-v, --version`            | Display version information                                                                                                            |
| `-c, --codes`              | Display language codes                                                                                                                 |
| `-s, --source <code>`      | Specify the source language (by default, engine will try to determine the language on its own)                                         |
| `-t, --translation <code>` | Specify the translation language(s), splitted by '+' (by default, the system language is used)                                         |
| `-e, --engine <engine>`    | Specify the translator engine ('google', 'yandex', 'deepl', 'duckduckgo', 'libre', 'mymemory' or 'reverso'), Google is used by default |
| `-u, --url <URL>`          | Specify Mozhi instance URL, random instance URL will be used by default.                                                               |
| `-r, --speak-translation`  | Speak the translation                                                                                                                  |
| `-o, --speak-source`       | Speak the source                                                                                                                       |
| `-f, --file`               | Read source text from files. Arguments will be interpreted as file paths                                                               |
| `-i, --stdin`              | Add stdin data to source text                                                                                                          |
| `-a, --audio-only`         | Print text only for speaking when using `--speak-translation` or `--speak-source`                                                      |
| `-b, --brief`              | Print only translations                                                                                                                |
| `-j, --json`               | Print output formatted as JSON                                                                                                         |

**Note:** If you do not pass startup arguments to the program, the GUI starts.

## D-Bus API

    org.kde.CrowTranslate
    ├── /org/kde/CrowTranslate/Ocr
    |   └── method void org.kde.CrowTranslate.Ocr.setParameters(QVariantMap parameters);
    └── /org/kde/CrowTranslate/MainWindow
        |   # Global shortcuts
        ├── method void org.kde.CrowTranslate.MainWindow.translateSelection();
        ├── method void org.kde.CrowTranslate.MainWindow.speakSelection();
        ├── method void org.kde.CrowTranslate.MainWindow.speakTranslatedSelection();
        ├── method void org.kde.CrowTranslate.MainWindow.playPauseSpeaking();
        ├── method void org.kde.CrowTranslate.MainWindow.stopSpeaking();
        ├── method void org.kde.CrowTranslate.MainWindow.open();
        ├── method void org.kde.CrowTranslate.MainWindow.copyTranslatedSelection();
        ├── method void org.kde.CrowTranslate.MainWindow.recognizeScreenArea();
        ├── method void org.kde.CrowTranslate.MainWindow.translateScreenArea();
        ├── method void org.kde.CrowTranslate.MainWindow.delayedRecognizeScreenArea();
        ├── method void org.kde.CrowTranslate.MainWindow.delayedTranslateScreenArea();
        |   # Main window shortcuts
        ├── method void org.kde.CrowTranslate.MainWindow.clearText();
        ├── method void org.kde.CrowTranslate.MainWindow.cancelOperation();
        ├── method void org.kde.CrowTranslate.MainWindow.swapLanguages();
        ├── method void org.kde.CrowTranslate.MainWindow.openSettings();
        ├── method void org.kde.CrowTranslate.MainWindow.setAutoTranslateEnabled(bool enabled);
        ├── method void org.kde.CrowTranslate.MainWindow.copySourceText();
        ├── method void org.kde.CrowTranslate.MainWindow.copyTranslation();
        ├── method void org.kde.CrowTranslate.MainWindow.copyAllTranslationInfo();
        └── method void org.kde.CrowTranslate.MainWindow.quit();

For example, you can show main window using `dbus-send`:

```bash
dbus-send --type=method_call --dest=org.kde.CrowTranslate /org/kde/CrowTranslate/MainWindow org.kde.CrowTranslate.MainWindow.open
```

Or via `qdbus`:

```bash
qdbus org.kde.CrowTranslate /org/kde/CrowTranslate/MainWindow org.kde.CrowTranslate.MainWindow.open
# or shorter
qdbus org.kde.CrowTranslate /org/kde/CrowTranslate/MainWindow open
```

## Global shortcuts in wayland

Wayland doesn't provide API for global shortcuts and you need to register them by yourself. 

### KDE

KDE have a convenient feature to define shortcuts in `.desktop` file and import them in settings. We already defined these shortcuts in the `.desktop` file, you just need to activate it in settings.

Open Settings -> Keyboard -> Shortcuts -> Add new -> Application, select "Crow Translate" and press Ok.

If you migrating from 2.x, you will need to remove older entry first since the application `.desktop` file changed.

This method will also work for X11.

### GNOME

For GNOME you need to manually set D-Bus commands as global shortcuts. For example, to translate selected text use the following:

```bash
qdbus org.kde.CrowTranslate /org/kde/CrowTranslate/MainWindow translateSelection
```

You can set a hotkey for this command in GNOME system settings.

## OCR

The application can translate text from the screen using [Tesseract](https://github.com/tesseract-ocr/tesseract). To recognize text, you need to additionally install trained models and select at least one model in the settings. Choose only the models for the languages you need. The more models there are, the worse the performance and quality of OCR.

You can manually download models from the [tessdata_fast](https://github.com/tesseract-ocr/tessdata_fast) repository or install the `org.kde.CrowTranslate.tessdata` extension if you use the app from [Flatpak](https://flatpak.org).

## Dependencies

### Required

- [CMake](https://cmake.org) 3.16+
- [Extra CMake Modules](https://github.com/KDE/extra-cmake-modules)
- [Qt](https://www.qt.io) 5.9+ with Widgets, Network, Multimedia, Concurrent, X11Extras (Linux), DBus (Linux) and WinExtras (Windows) modules
- [Tesseract](https://tesseract-ocr.github.io) 4.0+
- [Png2Ico](https://www.winterdrache.de/freeware/png2ico) or [IcoTool](https://www.nongnu.org/icoutils) for generating executable icon (Windows)

### Optional

- [KWayland](https://invent.kde.org/frameworks/kwayland)

### External libraries

This project uses the following external libraries, which included as git submodules:

- [QHotkey](https://github.com/Skycoder42/QHotkey) - provides global shortcuts for desktop platforms.
- [SingleApplication](https://github.com/itay-grudev/SingleApplication) - prevents launch of multiple application instances.

## Icons

[Breeze](https://invent.kde.org/frameworks/breeze-icons) icon theme is bundled to provide icons on Windows and fallback icons on Linux.

[circle-flags](https://github.com/HatScripts/circle-flags "A collection of 300+ minimal circular SVG country flags") icons are used for flags.

## Installation

Downloads are available on the [KDE downloads](https://download.kde.org/stable/crow-translate) page. Also check out the [webpage](https://apps.kde.org/crow-translate) for other installation methods.

**Note:** On Linux to make the application look native on a non-KDE desktop environment, you need to configure Qt applications styling. This can be done by using [qt5ct](https://github.com/RomanVolak/qt5ct) or [adwaita-qt5](https://github.com/FedoraQt/adwaita-qt) or [qtstyleplugins](https://github.com/qt/qtstyleplugins). Please check the appropriate installation guide for your distribution.

**Note:** Windows requires [Microsoft Visual C++ Redistributable 2019](https://support.microsoft.com/en-us/topic/the-latest-supported-visual-c-downloads-2647da03-1eea-4433-9aff-95f26a218cc0) to work.

## Building

### Building executable

You can build **Crow Translate** by using the following commands:

```bash
mkdir build
cd build
cmake .. # Or `cmake -D CMAKE_BUILD_TYPE=Release ..` for single-configuration generators such as Ninja or GNU Make
cmake --build . # Or `cmake --build . --config Release` for multi-config generators such as Visual Studio Generators or Xcode
```

You will then get a binary named `crow`.

### Building a package using CPack

CMake can create [specified package types](https://cmake.org/cmake/help/latest/manual/cpack-generators.7.html) automatically.

If you use Makefile, Ninja, or Xcode generator you can use [package](https://cmake.org/cmake/help/latest/module/CPack.html#targets-package-and-package-source) target:

```bash
mkdir build
cd build
cmake -D CMAKE_BUILD_TYPE=Release -D CPACK_GENERATOR=DEB .. # You can specify several types of packages separated by semicolons in double quotes, for example: `CPACK_GENERATOR="DEB;ZIP;NSIS"`
cmake --build . --target package
```

Or you can use [CPack](https://cmake.org/cmake/help/latest/module/CPack.html) utility for any generators:

```bash
mkdir build
cd build
cmake .. # Or `cmake -D CMAKE_BUILD_TYPE=Release ..` for single-configuration generators such as Ninja or GNU Make
cpack -G DEB # Or `cpack -G DEB -C Release` for multi-config generators such as Visual Studio Generators or Xcode
```

You can also compile it using [Craft](https://community.kde.org/Craft):

```bash
craft crow-translate
```

### Build parameters

- `WITH_PORTABLE_MODE` - Enable portable functionality. If you create file named `settings.ini` in the app folder and Crow will store the configuration in it. It also adds the “Portable Mode” option to the application settings, which does the same.
- `WITH_KWAYLAND` - Find and use KWayland library for better Wayland integration.

Build parameters are passed at configuration stage: `cmake -D WITH_PORTABLE_MODE ..`.
