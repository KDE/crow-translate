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
#include "mockhttpserver.h"
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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

    // For the tests of an automatic destination with an unknown source, below.
    struct Guess {
        Language guess; // where the request goes before anything is known
        Language detected; // what the backend will be made to find
        Language preferred; // where the rule sends that
    };

    static Guess wrongGuess(const TranslationSession &session)
    {
        Guess g;
        g.guess = session.preferredDestination(Language(QLocale::system()));
        g.detected = g.guess == english() ? english() : german();
        g.preferred = session.preferredDestination(g.detected);
        return g;
    }

    static QByteArray mozhiAnswer(const QString &translation, const Language &detected)
    {
        return QJsonDocument(QJsonObject{{QStringLiteral("translated-text"), translation}, {QStringLiteral("detected"), detected.toCode()}})
            .toJson(QJsonDocument::Compact);
    }

    static QByteArray chatAnswer(const QString &content)
    {
        const QJsonObject message{{QStringLiteral("role"), QStringLiteral("assistant")}, {QStringLiteral("content"), content}};
        const QJsonObject choice{{QStringLiteral("index"), 0}, {QStringLiteral("message"), message}, {QStringLiteral("finish_reason"), QStringLiteral("stop")}};
        return QJsonDocument(QJsonObject{{QStringLiteral("choices"), QJsonArray{choice}}}).toJson(QJsonDocument::Compact);
    }

    static int processedCount(const QSignalSpy &states)
    {
        int count = 0;
        for (const QList<QVariant> &emission : states) {
            if (emission.at(0).value<ATranslationProvider::State>() == ATranslationProvider::State::Processed) {
                ++count;
            }
        }
        return count;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_configDir.isValid());
        // Written settings must not land in the real profile: this test sets
        // the language preference, and the auto rule is read back out of it.
        // An INI file, because the native format on Windows is the registry,
        // which setPath() does not move: under the application's own name,
        // this wrote into the real HKCU\Software\crow-translate.
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_configDir.path());
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

    // ---- an automatic destination, and a source nobody knows yet -----------
    //
    // "Auto" on both sides: the destination depends on the language of the
    // text, which only the provider can find out. The request goes out to a
    // guess, and what happens when the guess was wrong used to depend on who
    // asked and of which backend: the window asked again if the detection
    // arrived with a finished translation, the command line did its own
    // version of that, and a backend that detects first and translates
    // afterwards went on to the guess. German text read by someone whose
    // primary language is English came back in the secondary one.
    //
    // The guess is made for the system's language, which a test cannot choose,
    // so each of these picks the language to "detect" from where the guess
    // went: one for which the rule gives the other destination.

    // Mozhi: the language is detected by the request that translates. The
    // answer that went to the guess is never shown; the frontend hears of one
    // translation, in the language the rule gives.
    void testAWrongGuessIsAskedAgainWhenTheDetectionComesWithTheTranslation()
    {
        setPreference(english(), polish());
        MockHttpServer server;
        {
            AppSettings settings;
            settings.setInstance(server.baseUrl());
        }
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Mozhi);
        const Guess g = wrongGuess(session);
        QVERIFY(g.preferred != g.guess);
        server.queueJson(200, mozhiAnswer(QStringLiteral("into the guess"), g.detected));
        server.queueJson(200, mozhiAnswer(QStringLiteral("into the right language"), g.detected));

        QSignalSpy states(&session, &TranslationSession::translationStateChanged);
        session.requestTranslation(QStringLiteral("some text"), Language::autoLanguage(), Language::autoLanguage());

        QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(session.translator()->getState(), ATranslationProvider::State::Processed, 5000);
        QVERIFY2(server.requestPath(0).contains(QStringLiteral("to=") + g.guess.toCode()), qPrintable(server.requestPath(0)));
        QVERIFY2(server.requestPath(1).contains(QStringLiteral("to=") + g.preferred.toCode()), qPrintable(server.requestPath(1)));
        QCOMPARE(session.translator()->translationLanguage, g.preferred);
        QCOMPARE(session.translator()->result.translation, QStringLiteral("into the right language"));
        QCOMPARE(processedCount(states), 1);
        // ... and the language model agrees with what was translated.
        QCOMPARE(session.languages()->effectiveDestination(), g.preferred);
    }

    // The guess happened to be right: nothing is asked twice.
    void testARightGuessIsLeftAlone()
    {
        setPreference(english(), polish());
        MockHttpServer server;
        {
            AppSettings settings;
            settings.setInstance(server.baseUrl());
        }
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Mozhi);
        const Guess g = wrongGuess(session);
        // The language whose destination is the guess: the other one of the two.
        const Language confirming = g.detected == english() ? german() : english();
        QCOMPARE(session.preferredDestination(confirming), g.guess);
        server.queueJson(200, mozhiAnswer(QStringLiteral("into the guess"), confirming));

        session.requestTranslation(QStringLiteral("some text"), Language::autoLanguage(), Language::autoLanguage());

        QTRY_COMPARE_WITH_TIMEOUT(session.translator()->getState(), ATranslationProvider::State::Processed, 5000);
        QTest::qWait(200);
        QCOMPARE(server.requestCount(), 1);
        QCOMPARE(session.translator()->translationLanguage, g.guess);
    }

    // A destination the user chose is not the session's to change, whatever
    // the text turns out to be written in.
    void testAChosenDestinationIsNotFollowedUp()
    {
        setPreference(english(), polish());
        MockHttpServer server;
        {
            AppSettings settings;
            settings.setInstance(server.baseUrl());
        }
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Mozhi);
        server.queueJson(200, mozhiAnswer(QStringLiteral("auf Deutsch"), polish()));

        session.requestTranslation(QStringLiteral("some text"), german(), Language::autoLanguage());

        QTRY_COMPARE_WITH_TIMEOUT(session.translator()->getState(), ATranslationProvider::State::Processed, 5000);
        QTest::qWait(200);
        QCOMPARE(server.requestCount(), 1);
        QCOMPARE(session.translator()->translationLanguage, german());
    }

    // LocalAI with a detection model: it asks which language the text is in
    // before it asks for the translation. Nothing has gone to the guess yet
    // when the answer arrives, so nothing has to be asked twice - the one
    // translation goes where the rule says. It used to go to the guess.
    void testAWrongGuessIsRedirectedWhenTheBackendDetectsFirst()
    {
        setPreference(english(), polish());
        MockHttpServer server;
        {
            AppSettings settings;
            settings.setActiveLocalProvider(QStringLiteral("ollama"));
            settings.setLocalProviderUrl(QStringLiteral("ollama"), server.baseUrl());
            settings.setLocalProviderModel(QStringLiteral("ollama"), QStringLiteral("mock-model"));
            settings.setDetectProvider(QStringLiteral("ollama"));
            settings.setDetectModel(QStringLiteral("mock-model"));
        }
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::LocalAI);
        const Guess g = wrongGuess(session);
        QVERIFY(g.preferred != g.guess);
        server.queueJson(200, chatAnswer(g.detected.name() + QStringLiteral(": ") + g.detected.toCode().toUpper()));
        server.queueJson(200, chatAnswer(QStringLiteral("the translation")));

        QSignalSpy started(&session, &TranslationSession::translationStarted);
        session.requestTranslation(QStringLiteral("some text"), Language::autoLanguage(), Language::autoLanguage());

        QTRY_COMPARE_WITH_TIMEOUT(session.translator()->getState(), ATranslationProvider::State::Processed, 5000);
        QTest::qWait(200);
        // One detection, one translation, and that one into the right language.
        QCOMPARE(server.requestCount(), 2);
        const QString asked = QString::fromUtf8(server.requestBody(1));
        QVERIFY2(asked.contains(QStringLiteral("(%1)").arg(g.preferred.toCode())), qPrintable(asked));
        QVERIFY2(!asked.contains(QStringLiteral("(%1)").arg(g.guess.toCode())), qPrintable(asked));
        QCOMPARE(session.translator()->translationLanguage, g.preferred);
        QCOMPARE(session.languages()->effectiveDestination(), g.preferred);
        QCOMPARE(session.translator()->result.translation, QStringLiteral("the translation"));
    }

    // Aborting forgets the guess: a detection that still arrives afterwards
    // (or the next, unrelated one) must not start a translation of its own.
    void testAnAbortedRequestIsNotFollowedUp()
    {
        setPreference(english(), polish());
        MockHttpServer server;
        {
            AppSettings settings;
            settings.setInstance(server.baseUrl());
        }
        TranslationSession session;
        session.setTranslationBackend(ATranslationProvider::ProviderBackend::Mozhi);
        const Guess g = wrongGuess(session);
        MockHttpServer::Response held;
        held.hang = true;
        server.queueResponse(held);

        session.requestTranslation(QStringLiteral("some text"), Language::autoLanguage(), Language::autoLanguage());
        QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 5000);
        session.abortTranslation();

        // The same provider, asked by someone who names both languages, and
        // finding the text to be in the language the guess was wrong for.
        server.queueJson(200, mozhiAnswer(QStringLiteral("chosen"), g.detected));
        session.requestTranslation(QStringLiteral("other text"), german(), Language::autoLanguage());
        QTRY_COMPARE_WITH_TIMEOUT(session.translator()->getState(), ATranslationProvider::State::Processed, 5000);
        QTest::qWait(200);
        QCOMPARE(server.requestCount(), 2);
        QCOMPARE(session.translator()->translationLanguage, german());
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

    // Recognition has its own endpoint, set on its own settings page. The
    // address typed there was saved and never read: recognition went to the
    // translation endpoint, whatever the page said.
    void testLlmOcrUsesItsOwnEndpoint()
    {
        MockHttpServer translation;
        MockHttpServer recognition;
        recognition.queueJson(200, QByteArrayLiteral(R"({"choices":[{"message":{"role":"assistant","content":"READ IT"}}]})"));
        {
            AppSettings settings;
            settings.setOcrEngine(AppSettings::OcrEngine::Llm);
            settings.setOcrLlmProvider(QStringLiteral("openai_custom"));
            settings.setLocalProviderUrl(QStringLiteral("openai_custom"), translation.baseUrl());
            settings.setOcrLlmUrl(QStringLiteral("openai_custom"), recognition.baseUrl());
            settings.setOcrLlmModel(QStringLiteral("openai_custom"), QStringLiteral("mock-model"));
        }

        TranslationSession session;
        QVERIFY(session.prepareOcr());
        QSignalSpy recognized(session.llmOcr(), &AOcrProvider::recognized);
        QImage image(64, 32, QImage::Format_ARGB32);
        image.fill(Qt::white);
        session.llmOcr()->recognize(image, 96);

        QVERIFY(recognized.wait(5000));
        QCOMPARE(recognized.at(0).at(0).toString(), QStringLiteral("READ IT"));
        QCOMPARE(recognition.requestCount(), 1);
        QCOMPARE(translation.requestCount(), 0);
    }

    // Until the recognition page is first saved, the endpoint is the one
    // recognition always used, so an existing setup pointed at a remote
    // server keeps working. Its key goes with it, and only then.
    void testLlmOcrInheritsTheTranslationEndpointUntilItHasItsOwn()
    {
        const QString id = QStringLiteral("openai_custom");
        {
            AppSettings settings;
            QSettings raw;
            raw.remove(QStringLiteral("OcrLlm/") + id);
            settings.setLocalProviderUrl(id, QStringLiteral("http://translation.example:1234"));
            settings.setLocalProviderApiKey(id, QStringLiteral("translation-key"));
        }
        {
            const AppSettings settings;
            QCOMPARE(settings.ocrLlmUrl(id), QStringLiteral("http://translation.example:1234"));
            QCOMPARE(settings.ocrLlmApiKey(id), QStringLiteral("translation-key"));
        }
        {
            AppSettings settings;
            settings.setOcrLlmUrl(id, QStringLiteral("http://recognition.example:5678"));
        }
        const AppSettings settings;
        QCOMPARE(settings.ocrLlmUrl(id), QStringLiteral("http://recognition.example:5678"));
        QCOMPARE(settings.ocrLlmApiKey(id), QString()); // never sent to an address of its own
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
