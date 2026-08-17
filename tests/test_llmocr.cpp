/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// LlmOcr (vision-model OCR engine) contract: image in -> transcription out,
// never a translation. Pins the wire shape (multimodal chat-completions with
// an image part, transcription prompt) and that the OCR path is fully
// decoupled from LocalAiTranslationProvider.

#include "mockhttpserver.h"
#include "provideroptions.h"
#include "ocr/llmocr.h"
#include "translator/localaitranslationprovider.h"

#include <QCoreApplication>
#include <QImage>
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

} // namespace

class LlmOcrTest : public QObject
{
    Q_OBJECT

private slots:
    void testRecognizeSendsMultimodalTranscriptionRequest()
    {
        MockHttpServer server;
        Response response;
        response.status = 200;
        response.body = chatCompletionJson(QStringLiteral("Hello world"));
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("vision-model"));

        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(Qt::white);

        QSignalSpy spy(&ocr, &LlmOcr::recognized);
        ocr.recognize(image, 96);

        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.constFirst().at(0).toString(), QStringLiteral("Hello world"));

        QCOMPARE(server.requestCount(), 1);
        QVERIFY(server.requestPath(0).endsWith(QStringLiteral("/v1/chat/completions")));

        const QJsonObject body = QJsonDocument::fromJson(server.requestBody(0)).object();
        QCOMPARE(body.value(QStringLiteral("model")).toString(), QStringLiteral("vision-model"));

        const QJsonArray messages = body.value(QStringLiteral("messages")).toArray();
        QCOMPARE(messages.size(), 1);
        const QJsonArray content = messages.at(0).toObject().value(QStringLiteral("content")).toArray();
        QCOMPARE(content.size(), 2);
        QCOMPARE(content.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("text"));
        QCOMPARE(content.at(1).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("image_url"));

        const QString prompt = content.at(0).toObject().value(QStringLiteral("text")).toString();
        QVERIFY(prompt.contains(QStringLiteral("Transcribe"), Qt::CaseInsensitive));
    }

    void testCollapseRepeatedTranscription()
    {
        // The repeat-prone-local-model signature: the completed transcription
        // re-emitted verbatim, blank-line separated. Collapses to one copy.
        const QString block = QStringLiteral("System Monitor\nCPU 12% Memory 1.2GiB\nNotifications");
        const QString repeated = block + QStringLiteral("\n\n") + block + QStringLiteral("\n\n") + block;
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(repeated), block);

        // A single clean transcription is left untouched.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(block), block);

        // Empty / whitespace-only trims to empty.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(QString()), QString());
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(QStringLiteral(" ")), QString());

        // A single line looped to runaway length collapses.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(QStringLiteral("no\nno\nno\nno")), QStringLiteral("no"));

        // A multi-line block repeated twice now collapses too (2-copy bar,
        // user decision): a 2-line refrain poem doubled verbatim is the known,
        // accepted edge.
        const QString twoCopies = block + QStringLiteral("\n\n") + block;
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(twoCopies), block);

        // A short poem repeating one line three times stays intact (single
        // lines only read as runaway at four or more copies).
        const QString triple = QStringLiteral("Water\nWater\nWater");
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(triple), triple);

        // A poem that repeats its first stanza verbatim at the end (not an
        // exact tiling of the whole response) keeps every line.
        const QString enveloped = QStringLiteral("Once upon a time\nIn a land far away\nOnce upon a time");
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(enveloped), enveloped);

        // The real runaway signature ends mid-line when the token budget cuts
        // generation: full verbatim copies plus a partial block whose last
        // line is a prefix of the block's next line.
        const QString truncated = block + QStringLiteral("\n\n") + block + QStringLiteral("\n\n") + block
            + QStringLiteral("\n\nSystem Monitor\nCPU 12%");
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(truncated), block);

        // A single line repeated, ending in a line the token budget cut
        // mid-word (period=1 case) collapses to one line too.
        const QString singleLineLoop = QStringLiteral("Try integrating using the Ollama web service to create a locally-run personal code assistant.\n")
            + QStringLiteral("Try integrating using the Ollama web service to create a locally-run personal code assistant.\n")
            + QStringLiteral("Try integrating using the Ollama web service to create a locally-run personal code assistant.\n")
            + QStringLiteral("Try integrating using the Ollama web service to create a locally-run");
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(singleLineLoop),
                 QStringLiteral("Try integrating using the Ollama web service to create a locally-run personal code assistant."));
    }

    // A reasoning-capable vision model left with thinking on can spell out
    // its chain of thought inline before the final answer.
    // LocalAiTranslationProvider already asks such models to stop reasoning
    // via reasoning_effort; LlmOcr should offer the same knob for parity,
    // even though the concrete "text doubled in the source edit" bug this
    // engine shipped with (see testStopSequenceIsSentToPreventRunawayRepeat
    // below) turned out to be a different failure mode entirely - a small
    // local model looping on its own output with no clean stop token.
    void testDisableThinkingSendsReasoningEffortNone()
    {
        MockHttpServer server;
        Response response;
        response.body = chatCompletionJson(QStringLiteral("Hello world"));
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("glm-ocr"));
        ocr.setDisableThinking(true);

        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::black);

        QSignalSpy spy(&ocr, &LlmOcr::recognized);
        ocr.recognize(image, 96);
        QVERIFY(spy.wait(5000));

        const QJsonObject body = QJsonDocument::fromJson(server.requestBody(0)).object();
        QCOMPARE(body.value(QStringLiteral("reasoning_effort")).toString(), QStringLiteral("none"));
    }

    void testThinkingLeftAloneByDefault()
    {
        MockHttpServer server;
        Response response;
        response.body = chatCompletionJson(QStringLiteral("Hello world"));
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("glm-ocr"));

        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::black);

        QSignalSpy spy(&ocr, &LlmOcr::recognized);
        ocr.recognize(image, 96);
        QVERIFY(spy.wait(5000));

        const QJsonObject body = QJsonDocument::fromJson(server.requestBody(0)).object();
        QVERIFY(!body.contains(QStringLiteral("reasoning_effort")));
    }

    // The actual reported bug, reproduced live against a real Ollama +
    // glm-ocr in testLiveOllamaGlmOcrDoesNotRepeatTranscription (see
    // test_llmocr_live.cpp): a small local vision model finishes the correct
    // transcription, then - having no clean stop token for "done" - wraps it
    // in a markdown fence and loops re-emitting it until it runs out of
    // token budget. A stop sequence on the fence marker cuts generation the
    // instant that starts. This test only pins that the wire shape carries
    // the stop sequence; the live test proves it actually works.
    void testStopSequenceIsSentToPreventRunawayRepeat()
    {
        MockHttpServer server;
        Response response;
        response.body = chatCompletionJson(QStringLiteral("Hello world"));
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("glm-ocr"));

        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::black);

        QSignalSpy spy(&ocr, &LlmOcr::recognized);
        ocr.recognize(image, 96);
        QVERIFY(spy.wait(5000));

        const QJsonObject body = QJsonDocument::fromJson(server.requestBody(0)).object();
        const QJsonArray stop = body.value(QStringLiteral("stop")).toArray();
        QCOMPARE(stop.size(), 1);
        QCOMPARE(stop.first().toString(), QStringLiteral("```"));
    }

    void testCustomPromptIsSent()
    {
        MockHttpServer server;
        Response response;
        response.body = chatCompletionJson(QStringLiteral("text"));
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("m"));
        ocr.setPrompt(QStringLiteral("CUSTOM OCR PROMPT"));

        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::black);

        QSignalSpy spy(&ocr, &LlmOcr::recognized);
        ocr.recognize(image, 96);
        QVERIFY(spy.wait(5000));

        const QJsonObject body = QJsonDocument::fromJson(server.requestBody(0)).object();
        const QJsonArray content = body.value(QStringLiteral("messages")).toArray().at(0).toObject().value(QStringLiteral("content")).toArray();
        QCOMPARE(content.at(0).toObject().value(QStringLiteral("text")).toString(), QStringLiteral("CUSTOM OCR PROMPT"));
    }

    void testHttpErrorEmitsFailed()
    {
        MockHttpServer server;
        Response response;
        response.status = 500;
        response.body = QByteArrayLiteral("{}");
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("m"));

        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::black);

        QSignalSpy spy(&ocr, &LlmOcr::failed);
        ocr.recognize(image, 96);
        QVERIFY(spy.wait(5000));
        QVERIFY(!spy.constFirst().at(0).toString().isEmpty());
    }

    void testTimeoutEmitsFailed()
    {
        MockHttpServer server;
        Response hangResponse;
        hangResponse.hang = true;
        server.queueResponse(hangResponse);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("m"));
        ocr.setTimeout(1);

        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::black);

        QSignalSpy spy(&ocr, &LlmOcr::failed);
        ocr.recognize(image, 96);
        QVERIFY(spy.wait(5000));
        QVERIFY(spy.constFirst().at(0).toString().contains(QStringLiteral("timed out"), Qt::CaseInsensitive));
    }

    void testIsConfiguredRequiresUrlAndModel()
    {
        LlmOcr ocr;
        QVERIFY(!ocr.isConfigured());
        ocr.setEndpoint(QStringLiteral("http://localhost:11434"), false, QString());
        QVERIFY(!ocr.isConfigured());
        ocr.setModel(QStringLiteral("m"));
        QVERIFY(ocr.isConfigured());
    }

    // The OCR engine talks to the endpoint it was given, not to whatever the
    // translation provider happens to be configured with. Both are pointed at
    // mock servers here; only the OCR one may receive the request.
    void testUsesItsOwnEndpointNotTheTranslationProviders()
    {
        MockHttpServer ocrServer;
        MockHttpServer translationServer;
        Response response;
        response.status = 200;
        response.body = chatCompletionJson(QStringLiteral("transcribed"));
        ocrServer.queueResponse(response);

        LocalAiTranslationProvider translationProvider;
        auto options = translationProvider.getDefaultOptions();
        options->setOption("url", translationServer.baseUrl());
        options->setOption("model", QStringLiteral("translation-model"));
        translationProvider.applyOptions(*options);

        LlmOcr ocr;
        ocr.setEndpoint(ocrServer.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("vision-model"));

        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(Qt::white);

        QSignalSpy spy(&ocr, &LlmOcr::recognized);
        ocr.recognize(image, 96);
        QVERIFY(spy.wait(5000));

        QCOMPARE(ocrServer.requestCount(), 1);
        QCOMPARE(translationServer.requestCount(), 0);
        // And it asked the vision model, not the translation model.
        QVERIFY(ocrServer.requestBody(0).contains("vision-model"));
        QVERIFY(!ocrServer.requestBody(0).contains("translation-model"));
    }
};

QTEST_MAIN(LlmOcrTest)
#include "test_llmocr.moc"