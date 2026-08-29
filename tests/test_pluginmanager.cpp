/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// What a backend has to get right to be one, and what the host has to get
// right to survive one that is missing.
//
// Nothing here names a backend except where a settings file from 4.1.0 does:
// the conformance checks run over whatever is installed, so a plugin written
// somewhere else is held to the same rules.

#include "testisolation.h"
#include "core/pluginmanager.h"
#include "core/settingsstore.h"
#include "plugininterfaces/iocrplugin.h"
#include "plugininterfaces/itranslationplugin.h"
#include "plugininterfaces/ittsplugin.h"

#include <QCoreApplication>
#include <QSet>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace
{

constexpr PluginFamily allFamilies[] = {PluginFamily::Translation, PluginFamily::Tts, PluginFamily::Ocr};

QString familyName(PluginFamily family)
{
    switch (family) {
    case PluginFamily::Translation:
        return QStringLiteral("translation");
    case PluginFamily::Tts:
        return QStringLiteral("tts");
    case PluginFamily::Ocr:
        return QStringLiteral("ocr");
    }
    return QString();
}

// The options a provider will actually act on, asked of a real one.
QStringList acceptedOptions(PluginFamily family, const QString &id)
{
    PluginManager *manager = PluginManager::instance();
    switch (family) {
    case PluginFamily::Translation: {
        const std::unique_ptr<ATranslationProvider> provider(manager->createTranslationProvider(nullptr, id));
        return provider ? provider->getAvailableOptions() : QStringList();
    }
    case PluginFamily::Tts: {
        const std::unique_ptr<ATTSProvider> provider(manager->createTtsProvider(nullptr, id));
        return provider ? provider->getAvailableOptions() : QStringList();
    }
    case PluginFamily::Ocr: {
        const std::unique_ptr<AOcrProvider> provider(manager->createOcrProvider(nullptr, id));
        return provider ? provider->getAvailableOptions() : QStringList();
    }
    }
    return QStringList();
}

} // namespace

class PluginManagerTest : public QObject
{
    Q_OBJECT

private slots:
    // Something has to be there, or every check below passes vacuously.
    void testTheShippedBackendsAreFound()
    {
        PluginManager *manager = PluginManager::instance();
        QVERIFY(manager->contains(PluginFamily::Translation, QStringLiteral("copy")));
        QVERIFY(manager->contains(PluginFamily::Tts, QStringLiteral("none")));
        QVERIFY2(manager->available(PluginFamily::Translation).size() > 1,
                 "only the built-in translation backend was found - the modules are not being discovered");
        QVERIFY2(!manager->available(PluginFamily::Ocr).isEmpty(),
                 "no recognition backend was found at all");
    }

    // An id is what a settings file stores. Two backends sharing one within a
    // family means whichever loaded first silently wins.
    void testIdsAreUniqueWithinAFamily()
    {
        for (PluginFamily family : allFamilies) {
            QSet<QString> seen;
            const QList<PluginInfo> backends = PluginManager::instance()->available(family);
            for (const PluginInfo &backend : backends) {
                QVERIFY2(!backend.id.isEmpty(), qPrintable(QStringLiteral("a %1 backend declares no id").arg(familyName(family))));
                QVERIFY2(!seen.contains(backend.id),
                         qPrintable(QStringLiteral("two %1 backends both call themselves '%2'").arg(familyName(family), backend.id)));
                seen.insert(backend.id);
                QVERIFY2(backend.family == family,
                         qPrintable(QStringLiteral("backend '%1' was found under %2 but says it is something else").arg(backend.id, familyName(family))));
            }
        }
    }

    // Every backend has to load and describe itself, or it is a name in a
    // combo box that does nothing when chosen.
    void testEveryBackendDescribesItself()
    {
        for (PluginFamily family : allFamilies) {
            const QList<PluginInfo> backends = PluginManager::instance()->available(family);
            for (const PluginInfo &backend : backends) {
                QVERIFY2(PluginManager::instance()->describe(family, backend.id).has_value(),
                         qPrintable(QStringLiteral("backend '%1' could not be described - its module did not load").arg(backend.id)));
            }
        }
    }

    // The schema and the provider have to agree in both directions. An
    // option the provider takes but nobody declares can never be filled from
    // settings; one that is declared but ignored is a promise the settings
    // dialog makes and the backend does not keep.
    void testDeclaredOptionsAreTheOptionsTheProviderTakes()
    {
        for (PluginFamily family : allFamilies) {
            const QList<PluginInfo> backends = PluginManager::instance()->available(family);
            for (const PluginInfo &backend : backends) {
                const std::optional<PluginInfo> described = PluginManager::instance()->describe(family, backend.id);
                QVERIFY(described.has_value());

                QSet<QString> declared;
                for (const ProviderOptionSpec &spec : described->options)
                    declared.insert(spec.key);

                const QStringList accepted = acceptedOptions(family, backend.id);
                const QSet<QString> acceptedSet(accepted.cbegin(), accepted.cend());

                for (const QString &key : acceptedSet) {
                    QVERIFY2(declared.contains(key),
                             qPrintable(QStringLiteral("backend '%1' takes option '%2' but never declares it, so nothing can set it").arg(backend.id, key)));
                }
                for (const QString &key : declared) {
                    QVERIFY2(acceptedSet.contains(key),
                             qPrintable(QStringLiteral("backend '%1' declares option '%2' but does not take it").arg(backend.id, key)));
                }
            }
        }
    }

    // The acceptance criterion this whole thing has to meet: a machine with
    // the application installed and the backends not.
    void testWithNoBackendsInstalledItStillTranslates()
    {
#ifdef CROW_STATIC_PLUGINS
        QSKIP("The backends are compiled into this build; there is no plugin directory to take away");
#else
        QTemporaryDir empty;
        QVERIFY(empty.isValid());

        const QByteArray previous = qgetenv(PluginManager::searchPathEnvironmentVariable());
        qputenv(PluginManager::searchPathEnvironmentVariable(), empty.path().toLocal8Bit());
        PluginManager::instance()->reload();

        PluginManager *manager = PluginManager::instance();
        QCOMPARE(manager->available(PluginFamily::Translation).size(), 1);
        QVERIFY(manager->contains(PluginFamily::Translation, QStringLiteral("copy")));
        QCOMPARE(manager->available(PluginFamily::Tts).size(), 1);
        QVERIFY(manager->contains(PluginFamily::Tts, QStringLiteral("none")));
        QVERIFY(manager->available(PluginFamily::Ocr).isEmpty());

        // A backend that is not there falls back rather than failing, and the
        // one that is not there for OCR is simply absent - there has never
        // been a built-in engine to stand in for it.
        const std::unique_ptr<ATranslationProvider> translator(manager->createTranslationProvider(nullptr, QStringLiteral("mozhi")));
        QVERIFY(translator != nullptr);
        QCOMPARE(translator->getProviderType(), QStringLiteral("CopyTranslationProvider"));

        const std::unique_ptr<ATTSProvider> speaker(manager->createTtsProvider(nullptr, QStringLiteral("piper")));
        QVERIFY(speaker != nullptr);

        QVERIFY(manager->createOcrProvider(nullptr, QStringLiteral("tesseract")) == nullptr);

        qputenv(PluginManager::searchPathEnvironmentVariable(), previous);
        PluginManager::instance()->reload();
#endif
    }

    // A backend nobody in this tree compiled, found by scanning - and found in
    // both link modes.
    //
    // Compiling the shipped backends into the application is a statement about
    // *those* backends: an installer's tree is self-contained and a file in it
    // that can go missing is not a fallback. It says nothing about a backend
    // somebody else wrote. Discovery used to stop as soon as it had walked
    // Qt's static plugin table, which meant that on Windows, in the AppImage
    // and in every portable build - which is all of them the KDE CI produces -
    // a third-party module was unreachable wherever it was put.
    void testAModuleFromOutsideTheTreeIsFoundInEitherLinkMode()
    {
        const QByteArray previous = qgetenv(PluginManager::searchPathEnvironmentVariable());
        qputenv(PluginManager::searchPathEnvironmentVariable(), CROW_FOREIGN_PLUGIN_ROOT);
        PluginManager::instance()->reload();

        PluginManager *manager = PluginManager::instance();
        QVERIFY2(manager->contains(PluginFamily::Translation, QStringLiteral("testforeign")),
                 "a module sitting in the search path was not discovered; in a static build that is discover() stopping at the static plugin table");

        // Identity out of the JSON beside it, without the module having been
        // loaded to ask.
        const std::optional<PluginInfo> described = manager->describe(PluginFamily::Translation, QStringLiteral("testforeign"));
        QVERIFY(described.has_value());
        QCOMPARE(described->displayName, QStringLiteral("Foreign test backend"));

        // And it builds something. A module that is listed and cannot be
        // loaded is the failure this is really guarding against.
        const std::unique_ptr<ATranslationProvider> provider(manager->createTranslationProvider(nullptr, QStringLiteral("testforeign")));
        QVERIFY(provider != nullptr);
        QCOMPARE(provider->getProviderType(), QStringLiteral("CopyTranslationProvider"));

        qputenv(PluginManager::searchPathEnvironmentVariable(), previous);
        PluginManager::instance()->reload();
    }

    // A settings file written by 4.1.0 stored an ordinal into an enum that no
    // longer exists. It has to keep selecting the same backends.
    void testASettingsFileFrom410StillSelectsTheSameBackends()
    {
        {
            const std::unique_ptr<QSettings> settings(CrowSettings::open());
            settings->remove(QStringLiteral("Translation/Provider"));
            settings->remove(QStringLiteral("TTS/Provider"));
            settings->remove(QStringLiteral("OCR/EngineId"));
            // Mozhi, Piper, and the vision-model engine, as 4.1.0 wrote them.
            settings->setValue(QStringLiteral("Translation/Backend"), 1);
            settings->setValue(QStringLiteral("TTS/Backend"), 3);
            settings->setValue(QStringLiteral("OCR/Engine"), 1);
            settings->sync();
        }

        QCOMPARE(PluginManager::selectedId(PluginFamily::Translation), QStringLiteral("mozhi"));
        QCOMPARE(PluginManager::selectedId(PluginFamily::Tts), QStringLiteral("piper"));
        QCOMPARE(PluginManager::selectedId(PluginFamily::Ocr), QStringLiteral("llm"));

        // And it is resolved on read, not migrated: the old key is untouched,
        // so a downgrade still finds what it wrote.
        const std::unique_ptr<QSettings> settings(CrowSettings::open());
        QCOMPARE(settings->value(QStringLiteral("Translation/Backend")).toInt(), 1);
        QVERIFY(!settings->contains(QStringLiteral("Translation/Provider")));
    }

    // Choosing something writes the new key, and the new key wins.
    void testChoosingABackendWritesTheNewKey()
    {
        PluginManager::setSelectedId(PluginFamily::Translation, QStringLiteral("localai"));
        QCOMPARE(PluginManager::selectedId(PluginFamily::Translation), QStringLiteral("localai"));

        const std::unique_ptr<QSettings> settings(CrowSettings::open());
        QCOMPARE(settings->value(QStringLiteral("Translation/Provider")).toString(), QStringLiteral("localai"));
    }
};

int main(int argc, char *argv[])
{
    isolateTestSettings();

    QCoreApplication app(argc, argv);
    PluginManagerTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_pluginmanager.moc"
