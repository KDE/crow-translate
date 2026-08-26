/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "crow_core_export.h"
#include "plugininterfaces/iproviderplugin.h"

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

class AOcrProvider;
class ATTSProvider;
class ATranslationProvider;
class IOcrPlugin;
class ITranslationPlugin;
class ITtsPlugin;
class QPluginLoader;

// Which backends exist, and building one.
//
// This is what the three parallel registries collapse into. The
// ProviderBackend enums, the switch factories that turned them into
// providers, and the per-provider settings readers all answered the same
// question - what backends are there, and how do I get one - once each, in a
// place that had to be edited to add anything.
//
// Reads like FrontendRegistry one layer up, which said in its own header that
// a QPluginLoader-backed implementation could replace its body without any
// caller changing. This is that implementation, for the layer where it
// actually buys something: a backend can now be shipped by somebody who is
// not us.
//
// Two backends are not loadable, on purpose. Copy and the do-nothing TTS are
// what the host falls back to when a plugin throws or when the plugin
// directory is missing entirely, and a fallback that can itself be missing is
// not a fallback. They are compiled in and registered here.
class CROW_CORE_EXPORT PluginManager
{
    Q_DECLARE_TR_FUNCTIONS(PluginManager)

public:
    static PluginManager *instance();

    ~PluginManager();
    PluginManager(const PluginManager &) = delete;
    PluginManager &operator=(const PluginManager &) = delete;

    // Identity only - id, name, description, icon, capabilities - read from
    // each module's metadata without dlopening it. Enough to fill a combo
    // box; the options schema is not in here, see describe().
    QList<PluginInfo> available(PluginFamily family) const;
    bool contains(PluginFamily family, const QString &id) const;

    // The whole description, options included. Loads the module.
    std::optional<PluginInfo> describe(PluginFamily family, const QString &id) const;

    // The plugin object, for the two questions a PluginInfo cannot answer:
    // building a provider, and asking an OCR engine what languages its data
    // directory holds. Owned by the manager; never delete one.
    ITranslationPlugin *translationPlugin(const QString &id) const;
    ITtsPlugin *ttsPlugin(const QString &id) const;
    IOcrPlugin *ocrPlugin(const QString &id) const;

    // Build a provider, or fall back when the chosen backend cannot be built.
    //
    // The try/catch that used to live in each factory is here, and matters
    // more now than it did: a module that throws from its constructor is no
    // longer a bug in this repository. On failure the stored choice is reset
    // so the next start does not repeat it, and the user is told through
    // UserNotifier.
    //
    // Never returns nullptr for translation or TTS - there is always Copy and
    // always the do-nothing voice. May return nullptr for OCR, which has no
    // built-in engine and never pretended to.
    ATranslationProvider *createTranslationProvider(QObject *parent, const QString &id) const;
    ATTSProvider *createTtsProvider(QObject *parent, const QString &id) const;
    AOcrProvider *createOcrProvider(QObject *parent, const QString &id) const;

    // --- which backend is selected ------------------------------------------

    // The stored choice, as a plugin id.
    //
    // 4.1.0 and everything before it stored an enum ordinal. That is resolved
    // here on read and never written back, the same way 5bfc9f80 chose to
    // resolve rather than migrate: a settings file stays readable by the
    // version that wrote it until the user changes something.
    static QString selectedId(PluginFamily family);
    static void setSelectedId(PluginFamily family, const QString &id);

    // What to select when nothing is stored, or when what is stored is gone.
    // Answered by what is installed rather than by what was compiled in,
    // which is the same question the #ifdef WITH_PIPER_TTS in
    // AppSettings::defaultTTSProviderBackend() was asking badly.
    QString defaultId(PluginFamily family) const;

    // Copy, the do-nothing voice, and - for OCR - nothing at all.
    static QString fallbackId(PluginFamily family);

    // --- discovery ----------------------------------------------------------

    // Directories searched for this family's modules, in order.
    static QStringList searchPaths(PluginFamily family);

    // The environment variable that replaces the search path outright. Set it
    // to a directory with no plugins in it to see what a broken installation
    // looks like; the tests do exactly that.
    static const char *searchPathEnvironmentVariable();

    // Discover again from scratch. Only tests need this.
    void reload();

private:
    PluginManager();

    struct Entry {
        PluginInfo identity;
        // Null for a built-in.
        QPluginLoader *loader = nullptr;
        // Set only for a built-in, and owned by this class; a loaded module's
        // instance belongs to its loader.
        QObject *builtIn = nullptr;
    };

    void discover();
    void clear();
    IProviderPlugin *resolve(PluginFamily family, const QString &id) const;
    void rejectSelection(PluginFamily family, const QString &id, const QString &reason) const;

    QHash<QString, Entry> m_entries[3];
    QList<QObject *> m_ownedBuiltIns;
};

#endif // PLUGINMANAGER_H
