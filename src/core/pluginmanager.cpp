/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/pluginmanager.h"

#include "core/builtinplugins.h"
#include "core/settingsstore.h"
#include "core/usernotifier.h"
#include "plugininterfaces/iocrplugin.h"
#include "plugininterfaces/itranslationplugin.h"
#include "plugininterfaces/ittsplugin.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QLibrary>
#include <QPluginLoader>
#include <QSettings>

#include <algorithm>
#include <memory>

namespace
{

struct FamilyTraits {
    PluginFamily family;
    // One directory per family under crowtranslate/, so a translation backend
    // and a TTS backend may share a name without sharing a file.
    const char *directory;
    const char *iid;
    const char *settingsKey;
    // What 4.1.0 and everything before it wrote: an ordinal into the
    // ProviderBackend enum. Frozen history rather than a registry - the list
    // never grows again, because nothing writes it any more.
    const char *legacyKey;
    const char *legacyIds[4];
    // Preference order for a fresh install, first one installed wins.
    const char *preferredIds[4];
};

constexpr FamilyTraits familyTraits[] = {
    {PluginFamily::Translation,
     "translation",
     CROW_TRANSLATION_PLUGIN_IID,
     "Translation/Provider",
     "Translation/Backend",
     {"copy", "mozhi", "localai", nullptr},
     {"mozhi", "localai", "copy", nullptr}},
    {PluginFamily::Tts,
     "tts",
     CROW_TTS_PLUGIN_IID,
     "TTS/Provider",
     "TTS/Backend",
     {"none", "mozhi", "qt", "piper"},
     // Piper always sounds better, even on Windows - and which of these is
     // actually here is now a question about the installation rather than
     // about WITH_PIPER_TTS at the time this was compiled.
     {"piper", "qt", "mozhi", "none"}},
    {PluginFamily::Ocr,
     "ocr",
     CROW_OCR_PLUGIN_IID,
     "OCR/EngineId",
     "OCR/Engine",
     {"tesseract", "llm", nullptr, nullptr},
     {"tesseract", "llm", nullptr, nullptr}},
};

const FamilyTraits &traitsFor(PluginFamily family)
{
    return familyTraits[static_cast<int>(family)];
}

int indexOf(PluginFamily family)
{
    return static_cast<int>(family);
}

PluginInfo identityFromMetaData(const QJsonObject &metaData, PluginFamily family)
{
    PluginInfo info;
    info.id = metaData.value(QLatin1String("id")).toString();
    info.displayName = metaData.value(QLatin1String("displayName")).toString();
    info.description = metaData.value(QLatin1String("description")).toString();
    info.iconName = metaData.value(QLatin1String("iconName")).toString();
    info.family = family;

    QStringList capabilities;
    const QJsonArray declared = metaData.value(QLatin1String("capabilities")).toArray();
    capabilities.reserve(declared.size());
    for (const QJsonValue &value : declared)
        capabilities.append(value.toString());
    info.capabilities = capabilitiesFromNames(capabilities);

    if (info.displayName.isEmpty())
        info.displayName = info.id;
    return info;
}

IProviderPlugin *asProviderPlugin(QObject *object, PluginFamily family)
{
    if (object == nullptr)
        return nullptr;

    switch (family) {
    case PluginFamily::Translation:
        return qobject_cast<ITranslationPlugin *>(object);
    case PluginFamily::Tts:
        return qobject_cast<ITtsPlugin *>(object);
    case PluginFamily::Ocr:
        return qobject_cast<IOcrPlugin *>(object);
    }
    return nullptr;
}

} // namespace

PluginManager *PluginManager::instance()
{
    static PluginManager manager;
    return &manager;
}

PluginManager::PluginManager()
{
    discover();
}

PluginManager::~PluginManager()
{
    clear();
}

const char *PluginManager::searchPathEnvironmentVariable()
{
    return "CROW_PLUGIN_PATH";
}

QStringList PluginManager::searchPaths(PluginFamily family)
{
    const QString suffix = QLatin1String("crowtranslate/") + QLatin1String(traitsFor(family).directory);

    // Replaces the search path rather than adding to it, so that "no plugins
    // at all" is expressible. The tests need that to check what a broken
    // installation does; nothing else should set it.
    const QByteArray override = qgetenv(searchPathEnvironmentVariable());
    if (!override.isEmpty()) {
        QStringList paths;
        const QStringList roots = QFile::decodeName(override).split(QDir::listSeparator(), Qt::SkipEmptyParts);
        paths.reserve(roots.size());
        for (const QString &root : roots)
            paths.append(QDir(root).filePath(QLatin1String("plugins/") + suffix));
        return paths;
    }

    QStringList paths;
    // Beside the executable first. A portable install has no system prefix to
    // look under, and a build tree wants the plugins it just built rather than
    // whatever is installed on the machine.
    paths.append(QDir(QCoreApplication::applicationDirPath()).filePath(QLatin1String("plugins/") + suffix));
#ifdef CROW_PLUGIN_INSTALL_DIR
    // Where this build installs them. Not the same as a Qt library path when
    // crow and Qt live under different prefixes, which on a distro they often
    // do not and in a container they often do.
    paths.append(QDir(QLatin1String(CROW_PLUGIN_INSTALL_DIR)).filePath(suffix));
#endif
    for (const QString &libraryPath : QCoreApplication::libraryPaths())
        paths.append(QDir(libraryPath).filePath(suffix));
    paths.removeDuplicates();
    return paths;
}

void PluginManager::discover()
{
    // Built-ins first, so that a module claiming to be "copy" cannot displace
    // the fallback everything else depends on.
    //
    // Their identity is written here rather than returned by them: a loadable
    // module keeps it in a JSON file beside itself, and these have no file.
    auto registerBuiltIn = [this](QObject *object, const PluginInfo &identity) {
        Q_ASSERT(asProviderPlugin(object, identity.family) != nullptr);
        Entry entry;
        entry.identity = identity;
        entry.builtIn = object;
        m_entries[indexOf(identity.family)].insert(identity.id, entry);
        m_ownedBuiltIns.append(object);
    };

    PluginInfo copy;
    copy.id = QStringLiteral("copy");
    // Deliberately still translated in SettingsDialog's context: the string is
    // unchanged and every translation of it is filed under that context.
    // Moving the code should not orphan the work.
    copy.displayName = QCoreApplication::translate("SettingsDialog", "Copy");
    copy.description = tr("Hands the text back unchanged. Always available.");
    copy.family = PluginFamily::Translation;
    registerBuiltIn(new CopyTranslationPlugin, copy);

    PluginInfo noop;
    noop.id = QStringLiteral("none");
    noop.displayName = QCoreApplication::translate("SettingsDialog", "None");
    noop.description = tr("Says nothing. Always available.");
    noop.family = PluginFamily::Tts;
    registerBuiltIn(new NoopTtsPlugin, noop);

    for (const FamilyTraits &traits : familyTraits) {
        const QStringList paths = searchPaths(traits.family);
        for (const QString &path : paths) {
            QDir directory(path);
            if (!directory.exists())
                continue;

            const QStringList fileNames = directory.entryList(QDir::Files | QDir::NoDotAndDotDot);
            for (const QString &fileName : fileNames) {
                if (!QLibrary::isLibrary(fileName))
                    continue;

                auto *loader = new QPluginLoader(directory.absoluteFilePath(fileName));
                const QJsonObject root = loader->metaData();
                if (root.value(QLatin1String("IID")).toString() != QLatin1String(traits.iid)) {
                    // Somebody else's plugin, or one built against an
                    // incompatible version of our interface. Either way not
                    // ours to load.
                    delete loader;
                    continue;
                }

                const PluginInfo identity = identityFromMetaData(root.value(QLatin1String("MetaData")).toObject(), traits.family);
                if (identity.id.isEmpty()) {
                    qWarning() << "PluginManager: ignoring" << loader->fileName() << "- its metadata declares no id";
                    delete loader;
                    continue;
                }
                if (m_entries[indexOf(traits.family)].contains(identity.id)) {
                    // Earlier search paths win, which is what makes a
                    // build tree or a portable directory able to shadow an
                    // installed copy.
                    delete loader;
                    continue;
                }

                Entry entry;
                entry.identity = identity;
                entry.loader = loader;
                m_entries[indexOf(traits.family)].insert(identity.id, entry);
            }
        }
    }
}

void PluginManager::clear()
{
    for (QHash<QString, Entry> &entries : m_entries) {
        for (Entry &entry : entries) {
            if (entry.loader != nullptr) {
                entry.loader->unload();
                delete entry.loader;
            }
        }
        entries.clear();
    }
    qDeleteAll(m_ownedBuiltIns);
    m_ownedBuiltIns.clear();
}

void PluginManager::reload()
{
    clear();
    discover();
}

QList<PluginInfo> PluginManager::available(PluginFamily family) const
{
    QList<PluginInfo> result;
    const QHash<QString, Entry> &entries = m_entries[indexOf(family)];
    result.reserve(entries.size());
    for (const Entry &entry : entries)
        result.append(entry.identity);

    std::sort(result.begin(), result.end(), [](const PluginInfo &left, const PluginInfo &right) {
        return left.displayName.localeAwareCompare(right.displayName) < 0;
    });
    return result;
}

bool PluginManager::contains(PluginFamily family, const QString &id) const
{
    return m_entries[indexOf(family)].contains(id);
}

IProviderPlugin *PluginManager::resolve(PluginFamily family, const QString &id) const
{
    const auto entry = m_entries[indexOf(family)].constFind(id);
    if (entry == m_entries[indexOf(family)].constEnd())
        return nullptr;

    if (entry->builtIn != nullptr)
        return asProviderPlugin(entry->builtIn, family);

    QObject *object = entry->loader->instance();
    if (object == nullptr) {
        qWarning() << "PluginManager: could not load" << entry->loader->fileName() << ":" << entry->loader->errorString();
        return nullptr;
    }
    return asProviderPlugin(object, family);
}

std::optional<PluginInfo> PluginManager::describe(PluginFamily family, const QString &id) const
{
    const auto entry = m_entries[indexOf(family)].constFind(id);
    if (entry == m_entries[indexOf(family)].constEnd())
        return std::nullopt;

    IProviderPlugin *plugin = resolve(family, id);
    if (plugin == nullptr)
        return std::nullopt;

    // Identity from the metadata, options from the object: the two halves are
    // written once each, in the place that can hold them.
    PluginInfo described = entry->identity;
    described.options = plugin->options();
    return described;
}

ITranslationPlugin *PluginManager::translationPlugin(const QString &id) const
{
    return dynamic_cast<ITranslationPlugin *>(resolve(PluginFamily::Translation, id));
}

ITtsPlugin *PluginManager::ttsPlugin(const QString &id) const
{
    return dynamic_cast<ITtsPlugin *>(resolve(PluginFamily::Tts, id));
}

IOcrPlugin *PluginManager::ocrPlugin(const QString &id) const
{
    return dynamic_cast<IOcrPlugin *>(resolve(PluginFamily::Ocr, id));
}

QString PluginManager::fallbackId(PluginFamily family)
{
    switch (family) {
    case PluginFamily::Translation:
        return QStringLiteral("copy");
    case PluginFamily::Tts:
        return QStringLiteral("none");
    case PluginFamily::Ocr:
        // No built-in engine, and none was ever pretended: "no OCR available"
        // is a state TranslationSession::prepareOcr() already reports.
        return QString();
    }
    return QString();
}

QString PluginManager::defaultId(PluginFamily family) const
{
    const FamilyTraits &traits = traitsFor(family);
    for (const char *candidate : traits.preferredIds) {
        if (candidate == nullptr)
            break;
        const QString id = QString::fromLatin1(candidate);
        if (contains(family, id))
            return id;
    }
    return fallbackId(family);
}

QString PluginManager::selectedId(PluginFamily family)
{
    const FamilyTraits &traits = traitsFor(family);
    const std::unique_ptr<QSettings> settings(CrowSettings::open());

    const QString stored = settings->value(QLatin1String(traits.settingsKey)).toString();
    if (!stored.isEmpty())
        return stored;

    // A settings file written by 4.1.0 or earlier. Resolved on read and never
    // written back: leaving the old key alone means downgrading still works,
    // and the file stops being ambiguous the first time the user picks
    // something. Same call 5bfc9f80 made for the language registry.
    const QVariant legacy = settings->value(QLatin1String(traits.legacyKey));
    if (legacy.isValid()) {
        bool ordinalIsValid = false;
        const int ordinal = legacy.toInt(&ordinalIsValid);
        const int legacyCount = int(sizeof(traits.legacyIds) / sizeof(traits.legacyIds[0]));
        if (ordinalIsValid && ordinal >= 0 && ordinal < legacyCount && traits.legacyIds[ordinal] != nullptr)
            return QString::fromLatin1(traits.legacyIds[ordinal]);
    }

    return PluginManager::instance()->defaultId(family);
}

void PluginManager::setSelectedId(PluginFamily family, const QString &id)
{
    const std::unique_ptr<QSettings> settings(CrowSettings::open());
    settings->setValue(QLatin1String(traitsFor(family).settingsKey), id);
}

void PluginManager::rejectSelection(PluginFamily family, const QString &id, const QString &reason) const
{
    qWarning() << "PluginManager: cannot use backend" << id << ":" << reason;

    UserNotifier::Notification notification;
    notification.severity = UserNotifier::Severity::Warning;
    notification.title = tr("Backend unavailable");
    notification.text = tr("The backend \"%1\" could not be used (%2). Falling back to the built-in one.").arg(id, reason);
    UserNotifier::notify(notification);
}

ATranslationProvider *PluginManager::createTranslationProvider(QObject *parent, const QString &id) const
{
    ITranslationPlugin *plugin = translationPlugin(id);
    if (plugin == nullptr) {
        // Not installed, or installed and unloadable. The stored choice is
        // deliberately left alone: installing the plugin package should bring
        // the user's backend back, not require them to pick it again.
        rejectSelection(PluginFamily::Translation, id, tr("not installed"));
        plugin = translationPlugin(fallbackId(PluginFamily::Translation));
    }

    try {
        ATranslationProvider *provider = plugin->createProvider(parent);
        if (provider != nullptr)
            return provider;
        rejectSelection(PluginFamily::Translation, id, tr("built nothing"));
    } catch (const std::exception &error) {
        // A throwing constructor is no longer a bug in this repository, and
        // the stored choice *is* reset here: repeating a crash on every start
        // is worse than losing a preference.
        rejectSelection(PluginFamily::Translation, id, QString::fromUtf8(error.what()));
        setSelectedId(PluginFamily::Translation, fallbackId(PluginFamily::Translation));
    } catch (...) {
        rejectSelection(PluginFamily::Translation, id, tr("crashed while starting"));
        setSelectedId(PluginFamily::Translation, fallbackId(PluginFamily::Translation));
    }

    return translationPlugin(fallbackId(PluginFamily::Translation))->createProvider(parent);
}

ATTSProvider *PluginManager::createTtsProvider(QObject *parent, const QString &id) const
{
    ITtsPlugin *plugin = ttsPlugin(id);
    if (plugin == nullptr) {
        rejectSelection(PluginFamily::Tts, id, tr("not installed"));
        plugin = ttsPlugin(fallbackId(PluginFamily::Tts));
    }

    try {
        ATTSProvider *provider = plugin->createProvider(parent);
        if (provider != nullptr)
            return provider;
        rejectSelection(PluginFamily::Tts, id, tr("built nothing"));
    } catch (const std::exception &error) {
        rejectSelection(PluginFamily::Tts, id, QString::fromUtf8(error.what()));
        setSelectedId(PluginFamily::Tts, fallbackId(PluginFamily::Tts));
    } catch (...) {
        rejectSelection(PluginFamily::Tts, id, tr("crashed while starting"));
        setSelectedId(PluginFamily::Tts, fallbackId(PluginFamily::Tts));
    }

    return ttsPlugin(fallbackId(PluginFamily::Tts))->createProvider(parent);
}

AOcrProvider *PluginManager::createOcrProvider(QObject *parent, const QString &id) const
{
    IOcrPlugin *plugin = ocrPlugin(id);
    if (plugin == nullptr) {
        // No fallback engine exists, and none is invented here. The caller
        // gets nullptr, which is the same "no recognition available" it
        // already handles.
        qWarning() << "PluginManager: no OCR engine" << id;
        return nullptr;
    }

    try {
        return plugin->createProvider(parent);
    } catch (const std::exception &error) {
        rejectSelection(PluginFamily::Ocr, id, QString::fromUtf8(error.what()));
    } catch (...) {
        rejectSelection(PluginFamily::Ocr, id, tr("crashed while starting"));
    }
    return nullptr;
}
