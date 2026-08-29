<!-- SPDX-FileCopyrightText: none -->
<!-- SPDX-License-Identifier: CC0-1.0 -->

# Writing a backend for Crow Translate

Crow Translate loads its translation, speech and text-recognition backends
rather than containing them. A backend is a Qt plugin over one small interface;
it can be built entirely outside this repository, against an installed Crow,
and dropped in.

There are working templates for all three families in
[`examples/plugins/`](examples/plugins/). Copy the one you want and change the
id. The rest of this file is what they are examples *of*.

## Content

- [The three families](#the-three-families)
- [Building one](#building-one)
- [The plugin class](#the-plugin-class)
- [The metadata file](#the-metadata-file)
- [Options and settings](#options-and-settings)
- [Where Crow looks](#where-crow-looks)
- [Windows, AppImages and portable builds](#windows-appimages-and-portable-builds)
- [What is promised, and for how long](#what-is-promised-and-for-how-long)
- [Failing well](#failing-well)
- [Licensing](#licensing)
- [Checking it works](#checking-it-works)

## The three families

| Family | Directory | Interface | Base class | IID |
| --- | --- | --- | --- | --- |
| Translation | `translation` | `ITranslationPlugin` | `ATranslationProvider` | `org.kde.CrowTranslate.TranslationProvider/1.0` |
| Speech | `tts` | `ITtsPlugin` | `ATTSProvider` | `org.kde.CrowTranslate.TtsProvider/1.0` |
| Recognition | `ocr` | `IOcrPlugin` | `AOcrProvider` | `org.kde.CrowTranslate.OcrProvider/1.0` |

Ids are unique within a family, not across them: Crow's own Mozhi backend both
translates and speaks, and ships as two backends that share a name.

Translation and speech have built-in fallbacks compiled into the application -
Copy and the silent voice - which are reached through exactly the same
interface as a loaded module and cannot themselves be uninstalled. Recognition
has none, and none is invented: with no engine installed, `crow --image` says
so.

## Building one

Install Crow Translate, then:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my-backend VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Crow REQUIRED)

crow_add_plugin_module(my-backend
    FAMILY translation          # or tts, or ocr
    SOURCES mybackendplugin.cpp mybackendprovider.cpp
)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=<crow prefix> -DCMAKE_INSTALL_PREFIX=<crow prefix>
cmake --build build
cmake --install build
```

`crow_add_plugin_module()` comes from the installed package and is a
convenience, not a requirement. What it does is the part that matters: a
`MODULE` library with AUTOMOC on, built into
`<build>/plugins/crowtranslate/<family>/`, installed into
`${Crow_PLUGIN_INSTALL_DIR}/<family>/`, with an rpath back to `libcrow-core`
and `--no-undefined` so that a symbol only the application has is a link error
rather than a load failure on somebody else's machine. Write those by hand if
you would rather.

`find_package(Crow)` also sets `Crow_PLUGIN_INSTALL_DIR`,
`Crow_CORE_LIBRARY_DIR` and `Crow_INCLUDE_DIR`, and defines the imported target
`Crow::Core`. Include paths are the same ones used inside Crow's own tree, so a
backend can be moved in or out of it without touching an `#include`:

```cpp
#include "plugininterfaces/itranslationplugin.h"
#include "translator/atranslationprovider.h"
```

Installing into Crow's own prefix is the simple case. Anywhere else works as
long as it lands in a directory Crow searches - see
[Where Crow looks](#where-crow-looks).

## The plugin class

Two classes, and keeping them apart is worth the extra file: the *provider* is
the backend, a plain `ATranslationProvider`/`ATTSProvider`/`AOcrProvider` that
can be linked into a test and driven without loading anything; the *plugin* is
the four lines that say how to get one.

```cpp
class MyTranslationPlugin : public QObject
    , public ITranslationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_TRANSLATION_PLUGIN_IID FILE "my.json")
    Q_INTERFACES(ITranslationPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    ATranslationProvider *createProvider(QObject *parent) override;
};
```

All three of `Q_OBJECT`, `Q_PLUGIN_METADATA` and `Q_INTERFACES` have to be
there, and each of them fails quietly on its own:

- no `Q_OBJECT`, or no moc: the module builds and declares no plugin at all;
- no `Q_PLUGIN_METADATA`, or the wrong IID: Crow reads the module's metadata,
  decides it is somebody else's, and skips it;
- no `Q_INTERFACES`: the module loads and then answers "not a translation
  plugin" to the cast.

Take the IID from the header rather than typing the string. It is the version
gate, and the point of it is that a module built against a later interface
stops being loaded rather than being loaded and found short a vtable entry.

The plugin object outlives every provider it builds. Ownership of the provider
belongs to the caller through `parent`; do not keep a pointer to it.

## The metadata file

The JSON named by `Q_PLUGIN_METADATA(... FILE ...)` is read **without loading
the module**, which is how a settings dialog lists every installed backend
without constructing any of them. Getting it wrong is therefore a bug that
never reaches your own code.

```json
{
    "id": "shouty",
    "displayName": "Shouty",
    "description": "Translates by shouting.",
    "iconName": "preferences-desktop-locale",
    "capabilities": ["LanguageDetection", "EngineSelection"]
}
```

| Key | Meaning |
| --- | --- |
| `id` | **Required.** Stable, lowercase, unique within the family. This is what goes in the user's settings file, so renaming it silently deselects the backend for everyone who had chosen it. |
| `displayName` | What the user sees. Defaults to `id`. |
| `description` | One line, shown beside the choice. |
| `iconName` | A themed icon name, or absent. |
| `capabilities` | Names from `ProviderCapability`: `LanguageDetection`, `EngineSelection`, `ProviderSelection`, `VoiceSelection`, `SpeakerSelection`. A name this build has never heard of is ignored rather than refused, so a plugin may claim a capability from a later version and simply not get it here. |

Whatever is declared here has to match what the provider's `capabilities()`
returns. The metadata is read before anything is loaded and the method after;
the two disagreeing shows up as a control that is missing or a control that
does nothing.

## Options and settings

A backend declares what it can be configured with. It never opens the settings
file: the host reads the values and hands them to `applyOptions()`, which is
what lets it configure a backend it has never heard of.

```cpp
QVector<ProviderOptionSpec> MyTranslationPlugin::options() const
{
    return {
        makeOptionSpec(QStringLiteral("endpoint"),   // the key applyOptions() sees
                       QString(),                    // settings key; empty means Plugins/<id>/endpoint
                       QMetaType::QString,
                       QStringLiteral("https://example.invalid"),
                       tr("Endpoint"),
                       tr("Where to send requests.")),
    };
}
```

- **`settingsKey` empty is the normal case for a new backend.** It gets
  `Plugins/<id>/<key>`, which is storage you do not have to invent. Crow's own
  backends name a key each because they had one before any of this existed and
  nobody's configuration was going to move for it.
- **`makeDirectoryOptionSpec()`** declares a path, which the settings dialog
  draws as a line edit with a browse button. The value arrives as a
  `QByteArray`.
- **`makeDerivedOptionSpec()`** declares a value the host computes and the user
  never sees.
- **`userVisible = false`** on an ordinary option means "the dialog draws this
  one by hand". Third-party backends have no hand-drawn page, so leaving it
  true is what gets you a settings page for free.

`options()` and the provider's `getAvailableOptions()` must agree in **both**
directions. An option the provider takes but nobody declares can never be set;
one that is declared and ignored is a promise the settings dialog makes and the
backend does not keep. Crow's own conformance test checks this over every
backend it can discover, including yours.

Which backend is selected lives in `Translation/Provider`, `TTS/Provider` and
`OCR/EngineId`, as plugin ids.

## Where Crow looks

For each family, in order, first match winning:

1. `$CROW_PLUGIN_PATH` - a list of roots, each searched at
   `<root>/plugins/crowtranslate/<family>`. This **replaces** the rest of the
   list rather than adding to it, which is what makes "no backends at all"
   expressible; it exists for tests, and nothing else should set it.
2. `<directory of the executable>/plugins/crowtranslate/<family>`.
3. The installed plugin directory, reached relatively from the executable - so
   a tree that has been moved, an AppImage included, still finds its own.
4. The plugin directory this build was configured with, absolutely.
5. Every `QCoreApplication::libraryPaths()` entry, at
   `<path>/crowtranslate/<family>`.

Backends compiled into the application are registered before any of this and
win an id collision, so a module cannot displace one.

## Windows, AppImages and portable builds

These builds set `CROW_STATIC_PLUGINS=ON`, which compiles the backends that
ship with Crow into the executable: an installer's tree is self-contained, and
a shipped fallback that can go missing is not a fallback.

That is a statement about *those* backends. Discovery still scans, so a
third-party module works the same way in these builds as in a distribution's.
On Windows that means:

```
<install>/bin/crow.exe
<install>/bin/crow-core.dll
<install>/bin/plugins/crowtranslate/translation/my-backend.dll   <- here
<install>/lib/crow-core.lib                                      <- link against this
```

`crow-core.dll` sits beside `crow.exe`, which is where the loader looks for a
plugin's dependencies; there is no rpath and none is needed. Build with the
same MSVC toolset and the same Qt as the Crow you are targeting - a plugin is a
C++ ABI boundary with Qt types crossing it, and MSVC will not tell you
otherwise until it crashes.

## What is promised, and for how long

- **The IID is the hard gate.** It carries a version, and it is bumped whenever
  an interface header changes in a way a compiled plugin would notice. A module
  with a stale IID is skipped at discovery. It is not loaded and then found
  wanting.
- **`libcrow-core` carries a soname of the project major version.**
- **Source and binary compatibility are promised within a minor series** -
  4.2.0 to 4.2.1, not 4.2 to 4.3. The installed
  `CrowConfigVersion.cmake` is written with `SameMinorVersion`, so building
  against a Crow that does not match fails at configure time, while there is
  still a log to read.
- **Only what carries `CROW_CORE_EXPORT` is API.** `libcrow-core` is built with
  hidden visibility, and the installed headers are the whole of the surface -
  deliberately narrower than what the library contains. If something you need
  is missing, that is a request worth making rather than a header worth
  copying.
- **Qt major version must match**, and on MSVC the toolset and C runtime as
  well.

Nothing here is promised about `crow-translate-lib`, `PluginManager`,
`TranslationSession`, `AppSettings` or anything else above the core. Those are
the application, and they move.

## Failing well

A backend runs inside somebody else's process and is expected to be survivable:

- **A throwing constructor is caught.** The host falls back to the built-in
  backend, tells the user which backend failed and why, and resets the stored
  choice - repeating a crash on every start is worse than losing a preference.
- **A backend that is simply not installed does not reset anything.**
  Installing the package again should bring the user's choice back.
- **Do not put up dialogs.** A backend has no idea whether there is a window to
  put one in; the same code runs under the CLI. `UserNotifier::notify()` says
  what happened and lets the frontend decide what that looks like - a message
  box in the GUI, a line on stderr in the terminal.
- **Do not return markup.** Fill in `TranslationResult`'s fields. Rendering
  belongs to the frontend, and a backend that returns HTML gets read out loud
  with the tags in it.
- **Emit the states.** A caller that asked for speech waits for
  `QTextToSpeech::Ready`; a recognition engine's `started()` is an interval the
  status strip shows. Emit transitions from the event loop rather than from
  inside the call that triggered them, or a caller that connected first and
  called second will wait for a signal it already missed.

## Licensing

`libcrow-core` is GPL-3.0-or-later, and a module that links it is a derived
work of it. The templates are licensed the same way for that reason, and a
backend built from one has to be GPL-3-compatible.

## Checking it works

```sh
crow --help                                  # your id appears in --tp / --tts
crow --tp <your id> -s en -t en "text"
crow --image picture.png --tp copy -s en -t en   # with OCR/EngineId set to yours
```

If the id is missing from `--help`, the module was not discovered: wrong
directory, wrong IID, or no `id` in the metadata. If it is listed and does not
run, it was discovered and failed to load; run with
`QT_LOGGING_RULES='*=true'` and Crow prints what `QPluginLoader` said.

The three templates are built and run exactly this way by Crow's own test
suite:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_OUT_OF_TREE_PLUGIN_TEST=ON
cmake --build build
ctest --test-dir build -R OutOfTreePlugin
```
