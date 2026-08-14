/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Regression tests for LocalAiTranslationProvider, covering gotchas #1-5
// from review-localai-backend.md (the review that shipped with MR !779).
// All requests go through MockHttpServer instead of a real Ollama/LM
// Studio/FastFlowLM instance.
//
// #4 is expected to FAIL on this branch (localai-backend+tests) - its fix
// (797fedec) only exists on localai-backend-salvage, which this branch
// doesn't have yet. That's intentional: it documents what the salvage merge
// needs to fix, not a bug in the test.

#include "language.h"
#include "mockhttpserver.h"
#include "provideroptions.h"
#include "testisolation.h"
#include "translator/atranslationprovider.h"
#include "translator/localaitranslationprovider.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using Response = MockHttpServer::Response;

namespace
{

QByteArray chatCompletionJson(const QString &content)
{
    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    message.insert(QStringLiteral("content"), content);
    QJsonObject choice;
    choice.insert(QStringLiteral("message"), message);
    QJsonArray choices;
    choices.append(choice);
    QJsonObject body;
    body.insert(QStringLiteral("choices"), choices);
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

std::unique_ptr<LocalAiTranslationProvider> makeProvider(const QString &baseUrl, int timeoutSeconds = 300)
{
    auto provider = std::make_unique<LocalAiTranslationProvider>();
    ProviderOptions opts = *provider->getDefaultOptions();
    opts.setOption(QStringLiteral("url"), baseUrl);
    opts.setOption(QStringLiteral("timeout"), timeoutSeconds);
    provider->applyOptions(opts);
    return provider;
}

} // namespace

class LocalAiProviderTest : public QObject
{
    Q_OBJECT

private slots:
    // The generic "does a golden-path translate reach Processed with
    // NoError/a non-empty result via Processing" assertion now lives in
    // tests/contract/test_translation_provider_contract.cpp, run against
    // every backend including this one. What's left here is purely
    // LocalAI's own wire format: it must hit /v1/chat/completions exactly
    // once and actually return the mocked content verbatim.
    void testGoldenPathTranslateUsesChatCompletionsEndpoint()
    {
        MockHttpServer server;
        server.queueJson(200, chatCompletionJson(QStringLiteral("Hola Mundo")));

        auto provider = makeProvider(server.baseUrl());
        provider->translate(QStringLiteral("Hello World"), Language(QStringLiteral("es")), Language(QStringLiteral("en")));

        QVERIFY(QTest::qWaitFor([&provider]() {
            return provider->getState() == ATranslationProvider::State::Processed;
        },
                                5000));
        QCOMPARE(provider->result, QStringLiteral("Hola Mundo"));
        QCOMPARE(server.requestCount(), 1);
        QVERIFY(server.requestPath(0).contains(QStringLiteral("/v1/chat/completions")));
    }

    // Gotcha #1: a request that never responds must not leave the provider
    // wedged in Processing forever - the transfer timeout must fire and
    // transition to Finished with an error, and a subsequent translate()
    // call must be accepted (not silently dropped by the Processing guard).
    void testTimeoutDoesNotWedgeProvider()
    {
        MockHttpServer server;
        server.queueResponse(Response{.hang = true});

        auto provider = makeProvider(server.baseUrl(), /*timeoutSeconds=*/1);
        QSignalSpy stateSpy(provider.get(), &ATranslationProvider::stateChanged);

        provider->translate(QStringLiteral("Hello"), Language(QStringLiteral("es")), Language(QStringLiteral("en")));
        QCOMPARE(provider->getState(), ATranslationProvider::State::Processing);

        QVERIFY(QTest::qWaitFor([&provider]() {
            return provider->getState() == ATranslationProvider::State::Finished;
        },
                                8000));
        QVERIFY(provider->error != ATranslationProvider::TranslationError::NoError);

        // Finished (not Processing) - a fresh translate() must be accepted.
        server.queueJson(200, chatCompletionJson(QStringLiteral("Hola")));
        provider->translate(QStringLiteral("Hi"), Language(QStringLiteral("es")), Language(QStringLiteral("en")));
        QVERIFY(QTest::qWaitFor([&provider]() {
            return provider->getState() == ATranslationProvider::State::Processed;
        },
                                5000));
        QCOMPARE(provider->result, QStringLiteral("Hola"));
    }

    // Gotcha #2's invariant on the current code: sendTranslation() replaces
    // m_reply before issuing a new request, and onReplyFinished() checks
    // sender() identity against the current m_reply - so a stale reply from
    // an aborted request that arrives late must be discarded, not allowed to
    // overwrite a result a newer request already produced.
    void testStaleAbortedReplyDiscardedAfterNewTranslation()
    {
        MockHttpServer server;
        server.queueResponse(Response{.hang = true}); // request A: held

        auto provider = makeProvider(server.baseUrl());
        QSignalSpy stateSpy(provider.get(), &ATranslationProvider::stateChanged);

        provider->translate(QStringLiteral("first"), Language(QStringLiteral("es")), Language(QStringLiteral("en")));
        QVERIFY(QTest::qWaitFor([&server]() {
            return server.requestCount() == 1;
        },
                                3000));

        provider->abort();
        QCOMPARE(provider->getState(), ATranslationProvider::State::Finished);

        server.queueJson(200, chatCompletionJson(QStringLiteral("segunda")));
        provider->translate(QStringLiteral("second"), Language(QStringLiteral("es")), Language(QStringLiteral("en")));
        QVERIFY(QTest::qWaitFor([&provider]() {
            return provider->getState() == ATranslationProvider::State::Processed;
        },
                                5000));
        QCOMPARE(provider->result, QStringLiteral("segunda"));

        // Let the stale request A finally "respond" - must not touch state/result.
        server.releaseHeldRequest(0, Response{.status = 200, .body = chatCompletionJson(QStringLiteral("STALE - should never be seen"))});
        QTest::qWait(300);
        QCOMPARE(provider->getState(), ATranslationProvider::State::Processed);
        QCOMPARE(provider->result, QStringLiteral("segunda"));
    }

    // Gotcha #4: buildPrompt() round-trips the language code through
    // QLocale, which silently collapses an unrecognized/custom code to the
    // C locale, corrupting the prompt with "You are a professional C ...".
    // Fixed by 797fedec on localai-backend-salvage.
    void testCustomLanguageCodeDoesNotCorruptPrompt()
    {
        Language::registerCustomLanguage(QStringLiteral("zzq"), QStringLiteral("Zzqian"));

        MockHttpServer server;
        server.queueJson(200, chatCompletionJson(QStringLiteral("translated")));

        auto provider = makeProvider(server.baseUrl());
        provider->translate(QStringLiteral("Hello"), Language(QStringLiteral("es")), Language(QStringLiteral("zzq")));

        QVERIFY(QTest::qWaitFor([&server]() {
            return server.requestCount() == 1;
        },
                                5000));
        const QByteArray body = server.requestBody(0);

        QVERIFY2(!body.contains("professional C "), qUtf8Printable(QStringLiteral("Prompt corrupted to C-locale for a custom language code:\n") + QString::fromUtf8(body)));
    }

    // Gotcha #5: the detection regex must take the FIRST match in the
    // model's response, not the last (fixed by using a single it.next()
    // instead of looping the QRegularExpressionMatchIterator to exhaustion).
    void testDetectionRegexTakesFirstMatch()
    {
        MockHttpServer server;
        server.queueJson(200, chatCompletionJson(QStringLiteral("en - this is english")));

        auto provider = makeProvider(server.baseUrl());
        ProviderOptions opts = *provider->getDefaultOptions();
        opts.setOption(QStringLiteral("url"), server.baseUrl());
        opts.setOption(QStringLiteral("detect_via_llm"), true);
        opts.setOption(QStringLiteral("detect_url"), server.baseUrl());
        opts.setOption(QStringLiteral("detect_model"), QStringLiteral("some-model"));
        provider->applyOptions(opts);

        QSignalSpy detectSpy(provider.get(), &ATranslationProvider::languageDetected);
        provider->translate(QStringLiteral("Hello"), Language(QStringLiteral("es")), Language::autoLanguage());

        QVERIFY(detectSpy.wait(5000));
        const Language detected = qvariant_cast<Language>(detectSpy.constFirst().at(0));
        QCOMPARE(detected.toCode(), QStringLiteral("en"));
    }
};

int main(int argc, char *argv[])
{
    // No display/GUI needed - LocalAiTranslationProvider is a plain QObject,
    // but a real event loop (QCoreApplication) is required for
    // QNetworkAccessManager and MockHttpServer's QTcpServer to actually
    // process I/O. Isolated org/app name since Language::registerCustomLanguage()
    // persists to AppSettings.
    isolateTestSettings();

    QCoreApplication app(argc, argv);
    LocalAiProviderTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_localai_provider.moc"
