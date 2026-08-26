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
#include "onlinetranslator.h"
#include "core/translationsession.h"
#include "ocr/aocrprovider.h"
#include "settings/appsettings.h"
#include "translator/atranslationprovider.h"
#include "translator/languageresolution.h"
#ifdef WITH_TTS
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
    void applyOptions(const ProviderOptions &) override
    {
    }
    QStringList getAvailableOptions() const override
    {
        return {};
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

        session.setTranslationBackend(QStringLiteral("copy"));
        QCOMPARE(changed.count(), 1);
        ATranslationProvider *first = session.translator();
        QVERIFY(first != nullptr);
        QCOMPARE(session.translationBackend(), QStringLiteral("copy"));

        // Asking for the backend that is already current does nothing at all.
        session.setTranslationBackend(QStringLiteral("copy"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(session.translator(), first);

        session.setTranslationBackend(QStringLiteral("mozhi"));
        QCOMPARE(changed.count(), 2);
        QVERIFY(session.translator() != first);
        QCOMPARE(session.translationBackend(), QStringLiteral("mozhi"));
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
        session.setTranslationBackend(QStringLiteral("mozhi"));

        // Asked by name, not by cast. The backend is a module now, and the
        // MozhiTranslationProvider inside it is not the same type as one
        // linked into this test would be - which is exactly why a session
        // test should not be linking a backend at all.
        QCOMPARE(session.translator()->getProviderType(), QStringLiteral("MozhiTranslationProvider"));
        QCOMPARE(session.translator()->property("instance").toString(), instance);
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
        session.setTtsBackend(QStringLiteral("mozhi"));

        QCOMPARE(session.tts()->getProviderType(), QStringLiteral("MozhiTTSProvider"));
        QCOMPARE(session.tts()->property("instance").toString(), instance);
    }
#endif

    void testTtsBackendSwapReplacesTheProvider()
    {
        TranslationSession session;
        QSignalSpy changed(&session, &TranslationSession::ttsProviderChanged);

        session.setTtsBackend(QStringLiteral("none"));
        QCOMPARE(changed.count(), 1);
        ATTSProvider *first = session.tts();
        QVERIFY(first != nullptr);

        session.setTtsBackend(QStringLiteral("none"));
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
        session.setTranslationBackend(QStringLiteral("mozhi"));

        QSignalSpy states(&session, &TranslationSession::translationStateChanged);
        session.setTranslationBackend(QStringLiteral("copy"));
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
        session.setTranslationBackend(QStringLiteral("mozhi"));
        session.setTranslationBackend(QStringLiteral("copy"));

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
        session.setTranslationBackend(QStringLiteral("copy"));

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
        session.setTranslationBackend(QStringLiteral("copy"));
        session.setSelectedLanguages(polish(), Language::autoLanguage());

        QSignalSpy started(&session, &TranslationSession::translationStarted);
        session.requestTranslationOfSelection(QStringLiteral("dzien dobry"));

        QCOMPARE(started.count(), 1);
        QCOMPARE(started.at(0).at(2).value<Language>(), polish());
        // Source is not the primary, so the rule answers with the primary.
        QCOMPARE(started.at(0).at(1).value<Language>(), english());
    }

    // ---- which sides a global action switches to Auto ----------------------
    //
    // The two settings and the backend, through TranslationLogic::forcedAuto.
    // The settings are on unless a user turned them off, and that is how these
    // run: the first test of this that does not switch them off first.

    void testForcingIsOnAsInstalledForABackendThatDetects()
    {
        {
            AppSettings settings;
            settings.setInstance(QStringLiteral("https://mozhi.example.invalid"));
            settings.setCurrentEngine(OnlineTranslator::Google);
            settings.setForceSourceAutodetect(AppSettings::defaultForceSourceAutodetect());
            settings.setForceTranslationAutodetect(AppSettings::defaultForceTranslationAutodetect());
        }
        TranslationSession session;
        session.setTranslationBackend(QStringLiteral("mozhi"));

        using TranslationLogic::ForcedAuto;
        using TranslationLogic::ForcedFlow;
        QCOMPARE(session.forcedAutoFor(ForcedFlow::TranslateSelection), (ForcedAuto{true, true}));
        QCOMPARE(session.forcedAutoFor(ForcedFlow::Recognition), (ForcedAuto{true, true}));
        QCOMPARE(session.forcedAutoFor(ForcedFlow::SpeakSelection), (ForcedAuto{true, false}));
    }

    // DeepL through Mozhi cannot find a language out, and neither can Copy:
    // forcing the source to Auto for them made every global action fail with
    // "Language detection is not supported". 3.1.0 asked the engine first.
    void testABackendThatCannotDetectKeepsItsSource()
    {
        {
            AppSettings settings;
            settings.setInstance(QStringLiteral("https://mozhi.example.invalid"));
            settings.setCurrentEngine(OnlineTranslator::Deepl);
            settings.setForceSourceAutodetect(true);
            settings.setForceTranslationAutodetect(true);
        }
        using TranslationLogic::ForcedAuto;
        using TranslationLogic::ForcedFlow;

        TranslationSession session;
        session.setTranslationBackend(QStringLiteral("mozhi"));
        QVERIFY(!session.translator()->supportsAutodetection());
        QCOMPARE(session.forcedAutoFor(ForcedFlow::TranslateSelection), (ForcedAuto{false, true}));
        QCOMPARE(session.forcedAutoFor(ForcedFlow::Recognition), (ForcedAuto{false, true}));

        session.setTranslationBackend(QStringLiteral("copy"));
        QCOMPARE(session.forcedAutoFor(ForcedFlow::TranslateSelection), (ForcedAuto{false, true}));

        AppSettings().setCurrentEngine(OnlineTranslator::Google);
    }

    void testTheUserCanTurnEitherSideOff()
    {
        {
            AppSettings settings;
            settings.setInstance(QStringLiteral("https://mozhi.example.invalid"));
            settings.setCurrentEngine(OnlineTranslator::Google);
        }
        using TranslationLogic::ForcedAuto;
        using TranslationLogic::ForcedFlow;
        TranslationSession session;
        session.setTranslationBackend(QStringLiteral("mozhi"));

        AppSettings settings;
        settings.setForceSourceAutodetect(false);
        settings.setForceTranslationAutodetect(true);
        QCOMPARE(session.forcedAutoFor(ForcedFlow::TranslateSelection), (ForcedAuto{false, true}));
        settings.setForceSourceAutodetect(true);
        settings.setForceTranslationAutodetect(false);
        QCOMPARE(session.forcedAutoFor(ForcedFlow::TranslateSelection), (ForcedAuto{true, false}));
        settings.setForceSourceAutodetect(false);
        QCOMPARE(session.forcedAutoFor(ForcedFlow::Recognition), (ForcedAuto{false, false}));

        settings.setForceSourceAutodetect(AppSettings::defaultForceSourceAutodetect());
        settings.setForceTranslationAutodetect(AppSettings::defaultForceTranslationAutodetect());
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

    // --- the language of a text that is not being translated ----------------
    //
    // Reading a selection out. 3.1.0 found out what language it was in and
    // then spoke; since 4.0.0 it was spoken in whatever the source side had
    // last come to - the language of the text before it, or the system's.

    // With the source on Auto and a backend that detects, the backend is
    // asked, once, and what it says is the answer - and what the language
    // model says the source is from then on, whatever was translated before.
    void testTheLanguageOfATextIsAskedOfABackendThatDetects()
    {
        setPreference(english(), polish());
        MockHttpServer server;
        {
            AppSettings settings;
            settings.setInstance(server.baseUrl());
            settings.setCurrentEngine(OnlineTranslator::Google);
        }
        TranslationSession session;
        session.setTranslationBackend(QStringLiteral("mozhi"));
        session.setSelectedLanguages(Language::autoLanguage(), Language::autoLanguage());

        // An English text has been translated: that is what the source "is".
        server.queueJson(200, mozhiAnswer(QStringLiteral("tekst"), english()));
        session.requestTranslation(QStringLiteral("a text"), Language::autoLanguage(), Language::autoLanguage());
        QTRY_COMPARE_WITH_TIMEOUT(session.translator()->getState(), ATranslationProvider::State::Processed, 5000);
        QCOMPARE(session.languages()->effectiveSource(), english());
        session.resetTranslator();
        const int before = server.requestCount();

        server.queueJson(200, mozhiAnswer(QString(), german()));
        QSignalSpy found(&session, &TranslationSession::sourceLanguageFound);
        Language modelWhenAnswered = Language::autoLanguage();
        connect(&session, &TranslationSession::sourceLanguageFound, &session, [&session, &modelWhenAnswered]() {
            modelWhenAnswered = session.languages()->effectiveSource();
        });
        session.findSourceLanguage(QStringLiteral("Guten Tag zusammen"));
        QCOMPARE(found.count(), 0); // it has to be asked

        QTRY_COMPARE_WITH_TIMEOUT(found.count(), 1, 5000);
        QCOMPARE(found.at(0).at(0).toString(), QStringLiteral("Guten Tag zusammen"));
        QCOMPARE(found.at(0).at(1).value<Language>(), german());
        // The language model has it by the time the answer goes out: a
        // frontend that speaks on hearing it picks its voice from there, and
        // an English voice reads German no better for being told "German".
        QCOMPARE(modelWhenAnswered, german());
        QCOMPARE(session.languages()->effectiveSource(), german());
        QCOMPARE(server.requestCount(), before + 1);
        QVERIFY2(server.requestPath(before).contains(QStringLiteral("from=auto")), qPrintable(server.requestPath(before)));

        // One question, one answer.
        QTest::qWait(300);
        QCOMPARE(found.count(), 1);
    }

    // A source the user chose is the answer. Nothing is sent anywhere.
    void testAChosenSourceIsTheLanguageWithoutAsking()
    {
        MockHttpServer server;
        {
            AppSettings settings;
            settings.setInstance(server.baseUrl());
            settings.setCurrentEngine(OnlineTranslator::Google);
        }
        TranslationSession session;
        session.setTranslationBackend(QStringLiteral("mozhi"));
        session.setSelectedLanguages(polish(), english());

        QSignalSpy found(&session, &TranslationSession::sourceLanguageFound);
        session.findSourceLanguage(QStringLiteral("Guten Tag zusammen"));
        QCOMPARE(found.count(), 1);
        QCOMPARE(found.at(0).at(1).value<Language>(), polish());
        QTest::qWait(200);
        QCOMPARE(server.requestCount(), 0);
    }

    // A backend that cannot detect is not asked either: the answer is the
    // best there is, at once.
    void testABackendThatCannotDetectIsNotAskedForTheLanguage()
    {
        MockHttpServer server;
        {
            AppSettings settings;
            settings.setInstance(server.baseUrl());
            settings.setCurrentEngine(OnlineTranslator::Deepl);
        }
        TranslationSession session;
        session.setTranslationBackend(QStringLiteral("mozhi"));
        session.setSelectedLanguages(Language::autoLanguage(), Language::autoLanguage());

        QSignalSpy found(&session, &TranslationSession::sourceLanguageFound);
        session.findSourceLanguage(QStringLiteral("Guten Tag zusammen"));
        QCOMPARE(found.count(), 1);
        QCOMPARE(found.at(0).at(1).value<Language>(), session.languages()->effectiveSource());
        QTest::qWait(200);
        QCOMPARE(server.requestCount(), 0);

        AppSettings().setCurrentEngine(OnlineTranslator::Google);
    }

    // A backend that is asked and never says - here a detection model that
    // does not answer in time - must not leave the question open: the text is
    // still read, in the best language there is.
    void testALanguageQuestionIsAnsweredWhenTheBackendGivesUp()
    {
        setPreference(english(), polish());
        MockHttpServer server;
        {
            AppSettings settings;
            settings.setActiveLocalProvider(QStringLiteral("ollama"));
            settings.setLocalProviderUrl(QStringLiteral("ollama"), server.baseUrl());
            settings.setLocalProviderModel(QStringLiteral("ollama"), QStringLiteral("mock-model"));
            settings.setLocalAiTimeout(QStringLiteral("ollama"), 1);
            settings.setDetectProvider(QStringLiteral("ollama"));
            settings.setDetectModel(QStringLiteral("mock-model"));
        }
        TranslationSession session;
        session.setTranslationBackend(QStringLiteral("localai"));
        session.setSelectedLanguages(Language::autoLanguage(), Language::autoLanguage());

        MockHttpServer::Response silence;
        silence.hang = true;
        server.queueResponse(silence);

        QSignalSpy found(&session, &TranslationSession::sourceLanguageFound);
        session.findSourceLanguage(QStringLiteral("Guten Tag zusammen"));
        QCOMPARE(found.count(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(found.count(), 1, 8000);
        QCOMPARE(found.at(0).at(0).toString(), QStringLiteral("Guten Tag zusammen"));
        QCOMPARE(found.at(0).at(1).value<Language>(), session.languages()->effectiveSource());

        AppSettings().setLocalAiTimeout(QStringLiteral("ollama"), 300);
    }

    // The same backend when it does answer.
    void testADetectionModelSaysWhatLanguageATextIsIn()
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
        session.setTranslationBackend(QStringLiteral("localai"));
        session.setSelectedLanguages(Language::autoLanguage(), Language::autoLanguage());

        server.queueJson(200, chatAnswer(QStringLiteral("de")));
        QSignalSpy found(&session, &TranslationSession::sourceLanguageFound);
        session.findSourceLanguage(QStringLiteral("Guten Tag zusammen"));
        QTRY_COMPARE_WITH_TIMEOUT(found.count(), 1, 5000);
        QCOMPARE(found.at(0).at(1).value<Language>(), german());
        QCOMPARE(server.requestCount(), 1);
    }

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
        session.setTranslationBackend(QStringLiteral("mozhi"));
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
        session.setTranslationBackend(QStringLiteral("mozhi"));
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
        session.setTranslationBackend(QStringLiteral("mozhi"));
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
        session.setTranslationBackend(QStringLiteral("localai"));
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
        session.setTranslationBackend(QStringLiteral("mozhi"));
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
        session.setTranslationBackend(QStringLiteral("copy"));
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
        session.setTranslationBackend(QStringLiteral("copy"));
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
        session.setTranslationBackend(QStringLiteral("copy"));
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
        session.setTranslationBackend(QStringLiteral("copy"));
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
            settings.setOcrEngineId(QStringLiteral("tesseract"));
        }
        QVERIFY(session.ocr() != nullptr);
        QCOMPARE(session.ocr()->engineName(), QStringLiteral("tesseract"));

        {
            AppSettings settings;
            settings.setOcrEngineId(QStringLiteral("llm"));
        }
        QVERIFY(session.ocr() != nullptr);
        QCOMPARE(session.ocr()->engineName(), QStringLiteral("llm"));
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
            settings.setOcrEngineId(QStringLiteral("llm"));
            settings.setOcrLlmProvider(QStringLiteral("ollama"));
            settings.setLocalProviderUrl(QStringLiteral("ollama"), QStringLiteral("http://127.0.0.1:11434"));
            settings.setOcrLlmModel(QStringLiteral("ollama"), QStringLiteral("some-vision-model"));
        }

        TranslationSession session;
        // Untouched, it knows nothing: the settings above have never been
        // read into it.
        QVERIFY(session.ocr() != nullptr);
        QVERIFY(!session.ocr()->isConfigured());

        QVERIFY(session.prepareOcr());
        QVERIFY(session.ocr()->isConfigured());
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
            settings.setOcrEngineId(QStringLiteral("llm"));
            settings.setOcrLlmProvider(QStringLiteral("openai_custom"));
            settings.setLocalProviderUrl(QStringLiteral("openai_custom"), translation.baseUrl());
            settings.setOcrLlmUrl(QStringLiteral("openai_custom"), recognition.baseUrl());
            settings.setOcrLlmModel(QStringLiteral("openai_custom"), QStringLiteral("mock-model"));
        }

        TranslationSession session;
        QVERIFY(session.prepareOcr());
        QVERIFY(session.ocr() != nullptr);
        QSignalSpy recognized(session.ocr(), &AOcrProvider::recognized);
        QImage image(64, 32, QImage::Format_ARGB32);
        image.fill(Qt::white);
        session.ocr()->recognize(image, 96);

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
        session.applyOcrOptions();

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
