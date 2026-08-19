/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Generic ATranslationProvider contract, checked against every backend
// automatically via the data-driven rows below - add a fixture in
// providertestfixtures.h and a row in addBackendRows() for a new backend,
// no new test bodies needed. Two of these (the finish/reset cascades)
// don't come from the abstract base class alone - they encode exactly how
// MainWindow drives a provider (see mainwindow.cpp's
// translatorStateChanged()/on_translateButton_clicked()), which is where
// most of this app's actual implicit provider contract lives.

#include "providertestfixtures.h"
#include "testisolation.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

namespace
{

std::unique_ptr<ProviderFixture> makeFixture(const QString &name)
{
    if (name == QLatin1String("Copy"))
        return std::make_unique<CopyProviderFixture>();
    if (name == QLatin1String("LocalAI"))
        return std::make_unique<LocalAiProviderFixture>();
    if (name == QLatin1String("Mozhi"))
        return std::make_unique<MozhiProviderFixture>();
    return nullptr;
}

} // namespace

class TranslationProviderContractTest : public QObject
{
    Q_OBJECT

private slots:
    void testInitialStateIsReady_data()
    {
        addBackendRows();
    }
    void testInitialStateIsReady()
    {
        QFETCH(QString, backend);
        auto fixture = makeFixture(backend);
        auto provider = fixture->createForSuccess();
        if (!provider) {
            QSKIP("Backend needs live network that isn't reachable right now");
        }
        QCOMPARE(provider->getState(), ATranslationProvider::State::Ready);
    }

    void testGetProviderTypeNonEmpty_data()
    {
        addBackendRows();
    }
    void testGetProviderTypeNonEmpty()
    {
        QFETCH(QString, backend);
        auto fixture = makeFixture(backend);
        auto provider = fixture->createForSuccess();
        if (!provider) {
            QSKIP("Backend needs live network that isn't reachable right now");
        }
        QVERIFY(!provider->getProviderType().isEmpty());
    }

    void testSupportedLanguagesNonEmpty_data()
    {
        addBackendRows();
    }
    void testSupportedLanguagesNonEmpty()
    {
        QFETCH(QString, backend);
        auto fixture = makeFixture(backend);
        auto provider = fixture->createForSuccess();
        if (!provider) {
            QSKIP("Backend needs live network that isn't reachable right now");
        }
        QVERIFY(!provider->supportedSourceLanguages().isEmpty());
        QVERIFY(!provider->supportedDestinationLanguages().isEmpty());
    }

    // mainwindow.cpp's translatorStateChanged() has a Q_UNREACHABLE() in
    // the branch that would fire if a provider ever reports Processed with
    // a non-NoError error - i.e. every backend must guarantee error ==
    // NoError whenever it transitions to Processed. A backend that
    // violated this would crash (debug builds) or hit undefined behavior
    // (release builds), not just fail a translation.
    void testGoldenPathReachesProcessedViaProcessingWithNoError_data()
    {
        addBackendRows();
    }
    void testGoldenPathReachesProcessedViaProcessingWithNoError()
    {
        QFETCH(QString, backend);
        auto fixture = makeFixture(backend);
        auto provider = fixture->createForSuccess();
        if (!provider) {
            QSKIP("Backend needs live network that isn't reachable right now");
        }

        QSignalSpy stateSpy(provider.get(), &ATranslationProvider::stateChanged);
        provider->translate(QStringLiteral("Hello"), fixture->targetLanguage(), fixture->sourceLanguage());

        const bool reachedTerminal = QTest::qWaitFor([&provider]() {
            return provider->getState() == ATranslationProvider::State::Processed
                || provider->getState() == ATranslationProvider::State::Finished;
        },
                                                     10000);
        if (!reachedTerminal) {
            QSKIP("Translation did not complete (network required)");
        }
        if (provider->getState() == ATranslationProvider::State::Finished) {
            QSKIP("Backend reported a translation error rather than succeeding (network required)");
        }

        QCOMPARE(provider->getState(), ATranslationProvider::State::Processed);
        QCOMPARE(provider->error, ATranslationProvider::TranslationError::NoError);
        QVERIFY(!provider->result.isEmpty());

        // Every backend must pass through Processing before the terminal
        // state - MainWindow's abort button and translate-button-disabled
        // state depend on seeing it.
        QVERIFY(stateSpy.count() >= 2);
        const auto firstState = qvariant_cast<ATranslationProvider::State>(stateSpy.constFirst().at(0));
        QCOMPARE(firstState, ATranslationProvider::State::Processing);
        const auto lastState = qvariant_cast<ATranslationProvider::State>(stateSpy.constLast().at(0));
        QCOMPARE(lastState, ATranslationProvider::State::Processed);
    }

    // A backend must record which languages it translated between, because
    // that record is the only fact anything downstream has: LanguageResolution
    // answers from it, and speech, the voice combo boxes and the auto button's
    // label all answer from LanguageResolution. A backend that leaves these on
    // autoLanguage() sends every one of them back to guessing, which is how an
    // English voice came to read a Russian translation.
    void testReportsTheLanguagesItTranslatedBetween_data()
    {
        addBackendRows();
    }
    void testReportsTheLanguagesItTranslatedBetween()
    {
        QFETCH(QString, backend);
        auto fixture = makeFixture(backend);
        auto provider = fixture->createForSuccess();
        if (!provider) {
            QSKIP("Backend needs live network that isn't reachable right now");
        }

        provider->translate(QStringLiteral("Hello"), fixture->targetLanguage(), fixture->sourceLanguage());

        const bool reachedTerminal = QTest::qWaitFor([&provider]() {
            return provider->getState() == ATranslationProvider::State::Processed
                || provider->getState() == ATranslationProvider::State::Finished;
        },
                                                     10000);
        if (!reachedTerminal || provider->getState() == ATranslationProvider::State::Finished) {
            QSKIP("Translation did not succeed (network required)");
        }

        QVERIFY2(provider->translationLanguage != Language::autoLanguage(),
                 "backend did not record the destination it translated into");
        QCOMPARE(provider->translationLanguage, fixture->targetLanguage());

        // The source may legitimately have been auto-detected, but by the time
        // the translation is done the backend has to know what it decided.
        QVERIFY2(provider->sourceLanguage != Language::autoLanguage(),
                 "backend did not record the source it translated from");
    }

    // Mirrors mainwindow.cpp:1056/1210 exactly: on a successful
    // translation, MainWindow never calls reset() directly from Processed
    // (that would route through abort()'s per-backend network-cancelling
    // side effects for no reason) - it calls finish() first (a lightweight
    // Processed->Finished hop with no side effects), and
    // translatorStateChanged()'s Finished case unconditionally emits
    // resetTranslator (-> reset(), Finished->Ready). Every backend must
    // come out the other end of that exact two-hop cascade back in Ready,
    // or the translate button never re-enables for a second translation.
    void testSuccessCascadeViaFinishThenResetReturnsToReady_data()
    {
        addBackendRows();
    }
    void testSuccessCascadeViaFinishThenResetReturnsToReady()
    {
        QFETCH(QString, backend);
        auto fixture = makeFixture(backend);
        auto provider = fixture->createForSuccess();
        if (!provider) {
            QSKIP("Backend needs live network that isn't reachable right now");
        }

        provider->translate(QStringLiteral("Hello"), fixture->targetLanguage(), fixture->sourceLanguage());
        const bool reachedTerminal = QTest::qWaitFor([&provider]() {
            return provider->getState() == ATranslationProvider::State::Processed
                || provider->getState() == ATranslationProvider::State::Finished;
        },
                                                     10000);
        if (!reachedTerminal || provider->getState() != ATranslationProvider::State::Processed) {
            QSKIP("Translation did not succeed (network required)");
        }

        provider->finish();
        QCOMPARE(provider->getState(), ATranslationProvider::State::Finished);
        QCOMPARE(provider->error, ATranslationProvider::TranslationError::NoError);

        provider->reset();
        QCOMPARE(provider->getState(), ATranslationProvider::State::Ready);
        QCOMPARE(provider->error, ATranslationProvider::TranslationError::NoError);

        // The translate button's enabling logic (updateTranslateButtonState())
        // and on_translateButton_clicked()'s assert() both depend on a
        // subsequent translate() actually being accepted here, not
        // silently dropped by a leftover Processing/Finished guard.
        provider->translate(QStringLiteral("Hello again"), fixture->targetLanguage(), fixture->sourceLanguage());
        QVERIFY(provider->getState() != ATranslationProvider::State::Ready);
    }

    // Mirrors mainwindow.cpp:1063-1067: on a failed translation,
    // translatorStateChanged()'s Finished case shows the error and then
    // unconditionally emits resetTranslator (-> reset()) - no finish()
    // call needed since the provider is already Finished. Every backend
    // must come back to a clean Ready state from this too.
    void testFailureCascadeViaResetReturnsToReady_data()
    {
        addBackendRows();
    }
    void testFailureCascadeViaResetReturnsToReady()
    {
        QFETCH(QString, backend);
        auto fixture = makeFixture(backend);
        Language source;
        Language target;
        auto provider = fixture->createForFailure(source, target);
        QVERIFY(provider != nullptr);

        provider->translate(QStringLiteral("Hello"), target, source);
        QVERIFY(QTest::qWaitFor([&provider]() {
            return provider->getState() == ATranslationProvider::State::Finished;
        },
                                10000));
        QVERIFY(provider->error != ATranslationProvider::TranslationError::NoError);

        provider->reset();
        QCOMPARE(provider->getState(), ATranslationProvider::State::Ready);
        QCOMPARE(provider->error, ATranslationProvider::TranslationError::NoError);
        QVERIFY(provider->result.isEmpty());
    }

    // Whatever went wrong, MainWindow renders it verbatim via
    // getErrorString() (translatorStateChanged()'s Finished case), so every
    // backend owes that funnel a non-empty, translated, human-readable
    // string. An empty one shows the user a bare "Error: " with no cause,
    // and an untranslated one leaks English into a localized UI - both have
    // happened, hence the check.
    void testFailureProducesTranslatedNonEmptyErrorString_data()
    {
        addBackendRows();
    }
    void testFailureProducesTranslatedNonEmptyErrorString()
    {
        QFETCH(QString, backend);
        auto fixture = makeFixture(backend);
        Language source;
        Language target;
        auto provider = fixture->createForFailure(source, target);
        QVERIFY(provider != nullptr);

        provider->translate(QStringLiteral("Hello"), target, source);
        QVERIFY(QTest::qWaitFor([&provider]() {
            return provider->getState() == ATranslationProvider::State::Finished;
        },
                                10000));
        QVERIFY(provider->error != ATranslationProvider::TranslationError::NoError);

        const QString message = provider->getErrorString();
        QVERIFY2(!message.isEmpty(), qPrintable(QStringLiteral("%1 reported an error with no message").arg(backend)));
        QVERIFY2(!message.trimmed().isEmpty(), qPrintable(QStringLiteral("%1 reported a blank message").arg(backend)));

        // Not a placeholder or a bare enum name dumped into the UI: the user
        // has to be able to read it. (Whether each string went through tr()
        // cannot be asked at runtime - Qt hands back the source text when no
        // catalogue is loaded - so that half is enforced by lupdate
        // extraction, not from here.)
        QVERIFY2(message.length() > 3, qPrintable(QStringLiteral("%1: '%2' is too terse to be a message").arg(backend, message)));
        QVERIFY2(!message.contains(QLatin1String("TranslationError")),
                 qPrintable(QStringLiteral("%1: '%2' leaks an enum name").arg(backend, message)));
    }

    void testAbortWhileProcessingReachesFinishedWithError_data()
    {
        addBackendRows();
    }
    void testAbortWhileProcessingReachesFinishedWithError()
    {
        QFETCH(QString, backend);
        auto fixture = makeFixture(backend);
        if (!fixture->supportsDeterministicMidFlightAbort()) {
            QSKIP("Backend has no deterministic way to hold a request mid-flight");
        }
        auto provider = fixture->createForHang();
        QVERIFY(provider != nullptr);

        provider->translate(QStringLiteral("Hello"), fixture->targetLanguage(), fixture->sourceLanguage());
        QCOMPARE(provider->getState(), ATranslationProvider::State::Processing);

        provider->abort();
        QCOMPARE(provider->getState(), ATranslationProvider::State::Finished);
        QVERIFY(provider->error != ATranslationProvider::TranslationError::NoError);
    }

private:
    static void addBackendRows()
    {
        QTest::addColumn<QString>("backend");
        QTest::newRow("Copy") << QStringLiteral("Copy");
        QTest::newRow("LocalAI") << QStringLiteral("LocalAI");
        QTest::newRow("Mozhi") << QStringLiteral("Mozhi");
    }
};

int main(int argc, char *argv[])
{
    // No display/GUI needed - providers are plain QObjects, but a real
    // event loop (QCoreApplication) is required for QNetworkAccessManager
    // and MockHttpServer's QTcpServer to actually process I/O.
    isolateTestSettings();

    QCoreApplication app(argc, argv);
    TranslationProviderContractTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_translation_provider_contract.moc"
