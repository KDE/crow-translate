/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// TranslationSession is the part of the application that a frontend does not
// get to define: which providers exist, what languages a request is between,
// and what a recognition chains into. These run it with no window at all,
// which is the whole claim being made - the window was the only thing that
// could do any of this before.

#include "cmake.h"
#include "modulestatus.h"
#include "core/translationsession.h"
#include "ocr/aocrprovider.h"
#include "ocr/llmocr.h"
#include "ocr/tesseractocr.h"
#include "settings/appsettings.h"
#include "translator/atranslationprovider.h"
#include "translator/languageresolution.h"
#include "translator/mozhitranslationprovider.h"
#ifdef WITH_TTS
#include "tts/mozhittsprovider.h"
#endif
#include "tts/attsprovider.h"

#include <QImage>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// The smallest AOcrProvider that can report a recognition. The session only
// ever talks to the base class's signals, so driving those directly keeps
// this independent of Tesseract and of any LLM setup.
class SessionStubOcr : public AOcrProvider
{
    Q_OBJECT
    Q_DISABLE_COPY(SessionStubOcr)

public:
    using AOcrProvider::AOcrProvider;

    QString engineName() const override
    {
        return QStringLiteral("stub");
    }
    bool isConfigured() const override
    {
        return true;
    }
    void recognize(const QImage &, int) override
    {
    }
    void cancel() override
    {
    }

    void reportRecognized(const QString &text)
    {
        emit recognized(text);
    }
};

class TranslationSessionTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_configDir;

    static Language english()
    {
        return Language(QLocale::English);
    }
    static Language german()
    {
        return Language(QLocale::German);
    }
    static Language polish()
    {
        return Language(QLocale::Polish);
    }

    // primary/secondary drive the auto rule, and the session reads them from
    // the settings rather than being told - so the test has to write them.
    static void setPreference(const Language &primary, const Language &secondary)
    {
        AppSettings settings;
        settings.setPrimaryLanguage(primary);
        settings.setSecondaryLanguage(secondary);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_configDir.isValid());
        // Written settings must not land in the real profile: this test sets
        // the language preference, and the auto rule is read back out of it.
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_configDir.path());
        QCoreApplication::setOrganizationName(QStringLiteral(PROJECT_NAME));
        QCoreApplication::setApplicationName(QStringLiteral(PROJECT_NAME));
    }

    // Nothing is built until a backend is chosen. The window used to read the
    // choice into an uninitialised enum member and then create the provider
    // from it, so "no backend yet" was not a state that existed.
    void testNoProviderBeforeABackendIsChosen()
    {
        const TranslationSession session;

        QCOMPARE(session.translator(), nullptr);
        QCOMPARE(session.tts(), nullptr);
        // These three outlive every swap and exist from the start.
        QVERIFY(session.options() != nullptr);
        QVERIFY(session.languages() != nullptr);
        QVERIFY(session.moduleStatus() != nullptr);
    }

    void testBackendSwapReplacesTheProvider()
    {
        TranslationSession session;
        QSignalSpy changed(&session, &TranslationSession::translatorChanged);

        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);
        QCOMPARE(changed.count(), 1);
        ATranslationProvider *first = session.translator();
        QVERIFY(first != nullptr);
        QCOMPARE(session.translationBackend(), ATranslationProvider::ProviderBackend::Copy);

        // Asking for the backend that is already current does nothing at all.
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(session.translator(), first);

        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Mozhi);
        QCOMPARE(changed.count(), 2);
        QVERIFY(session.translator() != first);
        QCOMPARE(session.translationBackend(), ATranslationProvider::ProviderBackend::Mozhi);
    }

    // A provider is configured from settings as part of being built. Both
    // frontends now rely on that implicitly - the command line used to do it
    // itself, with its own locally constructed options manager, and a second
    // one a few lines later for TTS.
    void testANewProviderArrivesConfigured()
    {
        const QString instance = QStringLiteral("https://mozhi.example.invalid");
        {
            AppSettings settings;
            settings.setInstance(instance);
        }

        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Mozhi);

        auto *mozhi = qobject_cast<MozhiTranslationProvider *>(session.translator());
        QVERIFY(mozhi != nullptr);
        QCOMPARE(mozhi->instance(), instance);
    }

#ifdef WITH_TTS
    // The same for speech. This is the one the command line depended on most
    // directly: it used to build its own ProviderOptionsManager on the stack
    // and apply the settings by hand, so nothing but this now stands between
    // a configured voice and a default one.
    void testANewTtsProviderArrivesConfigured()
    {
        const QString instance = QStringLiteral("https://mozhi.example.invalid");
        {
            AppSettings settings;
            settings.setInstance(instance);
        }

        TranslationSession session;
        session.setTtsBackend(ATTSProvider::ProviderBackend::Mozhi);

        auto *mozhi = qobject_cast<MozhiTTSProvider *>(session.tts());
        QVERIFY(mozhi != nullptr);
        QCOMPARE(mozhi->instance(), instance);
    }
#endif

    void testTtsBackendSwapReplacesTheProvider()
    {
        TranslationSession session;
        QSignalSpy changed(&session, &TranslationSession::ttsProviderChanged);

        session.setTtsBackend(ATTSProvider::ProviderBackend::None);
        QCOMPARE(changed.count(), 1);
        ATTSProvider *first = session.tts();
        QVERIFY(first != nullptr);

        session.setTtsBackend(ATTSProvider::ProviderBackend::None);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(session.tts(), first);
    }

    // The point of the session owning the providers: a subscriber wires up
    // once and keeps working across a swap. In the window this was four
    // connections that had to be unpicked and re-made by hand on every swap,
    // which is exactly the kind of list something eventually falls off.
    void testStateSubscriptionSurvivesASwap()
    {
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Mozhi);

        QSignalSpy states(&session, &TranslationSession::translationStateChanged);
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);
        states.clear();

        session.requestTranslation(QStringLiteral("hello"), english(), english());

        // Processing, then Processed - through a subscription made while a
        // different provider object was in place.
        QCOMPARE(states.count(), 2);
        QCOMPARE(states.at(0).at(0).value<ATranslationProvider::State>(), ATranslationProvider::State::Processing);
        QCOMPARE(states.at(1).at(0).value<ATranslationProvider::State>(), ATranslationProvider::State::Processed);
    }

    // A swap has to move the status model onto the new provider too, or the
    // status strip reports on an object nobody is using any more.
    void testSwapRebindsTheStatusModel()
    {
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Mozhi);
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);

        QSignalSpy statusChanged(session.moduleStatus(), &ModuleStatus::changed);
        session.requestTranslation(QStringLiteral("hello"), english(), english());

        // Bound to the provider that ran, the model reports the run. Left
        // bound to the discarded one it would report nothing at all, and the
        // status strip would sit there describing an object nobody is using.
        QVERIFY(statusChanged.count() > 0);
        // Copy finishes inside the call, so by now the model has settled.
        QCOMPARE(session.moduleStatus()->activity(ModuleStatus::Module::Translation), ModuleStatus::Activity::Idle);
    }

    // "Auto" is resolved here, once, rather than by each caller. The CLI got
    // this wrong for exactly as long as it had its own copy of the rule.
    void testAutoDestinationIsResolvedBeforeTheRequestGoesOut()
    {
        setPreference(english(), german());
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);

        QSignalSpy started(&session, &TranslationSession::translationStarted);
        session.requestTranslation(QStringLiteral("hello"), Language::autoLanguage(), english());

        QCOMPARE(started.count(), 1);
        // Source is the primary, so the rule falls through to the secondary.
        QCOMPARE(started.at(0).at(1).value<Language>(), german());
        // ... and the provider was told the resolved language, not "auto".
        QCOMPARE(session.translator()->translationLanguage, german());
    }

    void testSelectionRequestKeepsAutoAsAuto()
    {
        setPreference(english(), german());
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);
        session.setSelectedLanguages(polish(), Language::autoLanguage());

        QSignalSpy started(&session, &TranslationSession::translationStarted);
        session.requestTranslationOfSelection(QStringLiteral("dzien dobry"));

        QCOMPARE(started.count(), 1);
        QCOMPARE(started.at(0).at(2).value<Language>(), polish());
        // Source is not the primary, so the rule answers with the primary.
        QCOMPARE(started.at(0).at(1).value<Language>(), english());
    }

    // Armed, one recognition translates. The connection is a member rather
    // than a local because it has to survive the return; the bug it exists to
    // prevent is a snip abandoned with Escape leaving it armed, so the next
    // recognition of any kind translated unexpectedly.
    void testArmedRecognitionTranslatesExactlyOnce()
    {
        setPreference(english(), german());
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);
        session.setSelectedLanguages(english(), english());
        SessionStubOcr ocr;

        QSignalSpy started(&session, &TranslationSession::translationStarted);
        QSignalSpy chaining(&session, &TranslationSession::ocrTranslationChaining);
        session.armOcrTranslation(&ocr);
        QVERIFY(session.isOcrTranslationArmed());

        ocr.reportRecognized(QStringLiteral("hello"));
        QCOMPARE(started.count(), 1);
        QCOMPARE(started.at(0).at(0).toString(), QStringLiteral("hello"));
        QCOMPARE(chaining.count(), 1);
        // Firing disarms it, so it cannot fire twice.
        QVERIFY(!session.isOcrTranslationArmed());

        ocr.reportRecognized(QStringLiteral("again"));
        QCOMPARE(started.count(), 1);
    }

    void testDisarmedRecognitionDoesNotTranslate()
    {
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);
        session.setSelectedLanguages(english(), english());
        SessionStubOcr ocr;

        QSignalSpy started(&session, &TranslationSession::translationStarted);
        session.armOcrTranslation(&ocr);
        session.disarmOcrTranslation();
        QVERIFY(!session.isOcrTranslationArmed());

        ocr.reportRecognized(QStringLiteral("hello"));
        QCOMPARE(started.count(), 0);
    }

    // Arming twice must leave one armed connection, not two. Re-arming is
    // what every capture entry point does, and the damage a second one causes
    // is not a double translation - the first to fire disarms, which hides
    // that - but a connection disarm cannot reach. Cancelling would then
    // leave the capture armed, and the next recognition of any kind, from
    // anywhere, would translate: the abandoned-snip bug, back again.
    void testReArmingLeavesNothingBehindForDisarmToMiss()
    {
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);
        session.setSelectedLanguages(english(), english());
        SessionStubOcr ocr;

        QSignalSpy started(&session, &TranslationSession::translationStarted);
        session.armOcrTranslation(&ocr);
        session.armOcrTranslation(&ocr);
        session.disarmOcrTranslation();
        QVERIFY(!session.isOcrTranslationArmed());

        ocr.reportRecognized(QStringLiteral("hello"));
        QCOMPARE(started.count(), 0);
    }

    // What the provider reports has to reach the language model without a
    // window in the middle, or a headless frontend can never know what a
    // translation was actually between.
    void testProviderReportsReachTheLanguageModel()
    {
        setPreference(english(), german());
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Copy);
        session.setSelectedLanguages(Language::autoLanguage(), Language::autoLanguage());

        QVERIFY(!session.languages()->translatedSource().has_value());

        session.requestTranslation(QStringLiteral("hello"), polish(), english());

        QCOMPARE(session.languages()->translatedSource(), std::optional<Language>(english()));
        QCOMPARE(session.languages()->translatedDestination(), std::optional<Language>(polish()));
    }

    // Which engine is active follows the settings and can move between one
    // recognition and the next, so it is answered per call rather than kept.
    void testActiveOcrEngineFollowsTheSettings()
    {
        const TranslationSession session;

        {
            AppSettings settings;
            settings.setOcrEngine(AppSettings::OcrEngine::Tesseract);
        }
        QCOMPARE(session.ocr(), static_cast<AOcrProvider *>(session.tesseractOcr()));

        {
            AppSettings settings;
            settings.setOcrEngine(AppSettings::OcrEngine::Llm);
        }
        QCOMPARE(session.ocr(), static_cast<AOcrProvider *>(session.llmOcr()));
    }

    // The ordering inside prepareOcr() is the whole point of it existing.
    // LlmOcr::isConfigured() reports the state of the last configuration, not
    // the state of the settings - so asking before configuring rejects an
    // engine the user has just finished setting up, which is exactly when
    // they are most likely to try it.
    void testPrepareOcrConfiguresBeforeItChecks()
    {
        {
            AppSettings settings;
            settings.setOcrEngine(AppSettings::OcrEngine::Llm);
            settings.setOcrLlmProvider(QStringLiteral("ollama"));
            settings.setLocalProviderUrl(QStringLiteral("ollama"), QStringLiteral("http://127.0.0.1:11434"));
            settings.setOcrLlmModel(QStringLiteral("ollama"), QStringLiteral("some-vision-model"));
        }

        TranslationSession session;
        // Untouched, it knows nothing: the settings above have never been
        // read into it.
        QVERIFY(!session.llmOcr()->isConfigured());

        QVERIFY(session.prepareOcr());
        QVERIFY(session.llmOcr()->isConfigured());
    }

    // A configuration the user chose and that does not work is worth saying
    // so about. Whether it gets said, and how, is the frontend's business -
    // the session only reports it.
    void testChosenOcrLanguagesThatDoNotWorkAreReported()
    {
        {
            AppSettings settings;
            settings.setOcrLanguagesString(QByteArrayLiteral("definitely-not-a-language"));
        }

        TranslationSession session;
        QSignalSpy unavailable(&session, &TranslationSession::ocrLanguagesUnavailable);
        session.initTesseractFromSettings();

        QCOMPARE(unavailable.count(), 1);
        QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("definitely-not-a-language"));
    }

    // Nothing here may reach for a widget: the session has to work in a
    // process that has no GUI at all, which is what QTEST_GUILESS_MAIN below
    // actually proves for every test in this file.
    void testAbortAndResetAreSafeWithoutAProvider()
    {
        TranslationSession session;
        session.abortTranslation();
        session.acceptTranslation();
        session.resetTranslator();
        session.disarmOcrTranslation();
        session.requestTranslation(QStringLiteral("hello"), english(), english());
        QCOMPARE(session.translator(), nullptr);
    }
};

QTEST_GUILESS_MAIN(TranslationSessionTest)

#include "test_translationsession.moc"
