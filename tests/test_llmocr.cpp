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
#include <QStringList>
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

// The image in the report is a two-character sign reading "Chinese",
// U+4E2D U+6587. Spelled as escapes, here and below, so what this test means
// cannot depend on how a compiler decodes the file it is written in.
QString cjkWord()
{
    return QStringLiteral("\u4e2d\u6587");
}

QString repeatedCopies(const QString &unit, int copies, const QString &separator)
{
    QStringList parts;
    parts.reserve(copies);
    for (int copy = 0; copy < copies; ++copy) {
        parts.append(unit);
    }
    return parts.join(separator);
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

    // A model that starts repeating must be cut off mid-generation, not
    // indulged to the end of its token budget and cleaned up afterwards. On
    // the reported screenshot the runaway cost 3876 tokens and 22 seconds.
    void testStreamingAbortsOnceTheTranscriptionRepeats()
    {
        const QString line = QStringLiteral(
            "His music can be found in the award winning indie games Actual Sunlight, "
            "This Is The Police I & II, and Hacknet: Labyrinths, HBO's Vice Principals");

        MockHttpServer server;
        Response response;
        response.status = 200;
        // The transcription, then the model looping it. Splitting each copy
        // over several deltas is what a real stream looks like.
        QList<QByteArray> chunks;
        auto appendCopy = [&chunks](const QString &text) {
            for (int at = 0; at < text.size(); at += 40) {
                QJsonObject delta;
                delta.insert(QStringLiteral("content"), text.mid(at, 40));
                QJsonObject choice;
                choice.insert(QStringLiteral("delta"), delta);
                QJsonObject body;
                body.insert(QStringLiteral("choices"), QJsonArray{choice});
                chunks.append(QByteArrayLiteral("data: ") + QJsonDocument(body).toJson(QJsonDocument::Compact) + QByteArrayLiteral("\n\n"));
            }
        };
        appendCopy(line);
        for (int copy = 0; copy < 20; ++copy) {
            appendCopy(QStringLiteral("\n") + line);
        }
        chunks.append(QByteArrayLiteral("data: [DONE]\n\n"));
        response.streamChunks = chunks;
        response.chunkDelayMs = 2;
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("vision-model"));
        QSignalSpy recognizedSpy(&ocr, &AOcrProvider::recognized);
        QSignalSpy failedSpy(&ocr, &AOcrProvider::failed);

        QImage image(32, 32, QImage::Format_RGB32);
        image.fill(Qt::white);
        ocr.recognize(image, 96);

        QVERIFY(recognizedSpy.wait(10000));
        QCOMPARE(failedSpy.count(), 0);
        // Exactly one copy, and the request was cut off rather than drained.
        QCOMPARE(recognizedSpy.constFirst().at(0).toString(), line);
        QTRY_VERIFY(server.clientDisconnectedEarly());
    }

    // The same abort, for the unit length East Asian text actually produces.
    // Before the evidence rule this cut the stream only after 82 folded
    // characters and handed back fourteen copies of the word.
    void testStreamingAbortsOnARepeatedShortUnit()
    {
        const QString word = cjkWord();

        MockHttpServer server;
        Response response;
        response.status = 200;
        QList<QByteArray> chunks;
        for (int copy = 0; copy < 200; ++copy) {
            QJsonObject delta;
            delta.insert(QStringLiteral("content"), copy == 0 ? word : QStringLiteral("\n") + word);
            QJsonObject choice;
            choice.insert(QStringLiteral("delta"), delta);
            QJsonObject body;
            body.insert(QStringLiteral("choices"), QJsonArray{choice});
            chunks.append(QByteArrayLiteral("data: ") + QJsonDocument(body).toJson(QJsonDocument::Compact) + QByteArrayLiteral("\n\n"));
        }
        chunks.append(QByteArrayLiteral("data: [DONE]\n\n"));
        response.streamChunks = chunks;
        response.chunkDelayMs = 2;
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("vision-model"));
        QSignalSpy recognizedSpy(&ocr, &AOcrProvider::recognized);
        QSignalSpy failedSpy(&ocr, &AOcrProvider::failed);

        QImage image(32, 32, QImage::Format_RGB32);
        image.fill(Qt::white);
        ocr.recognize(image, 96);

        QVERIFY(recognizedSpy.wait(10000));
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(recognizedSpy.constFirst().at(0).toString(), word);
        QTRY_VERIFY(server.clientDisconnectedEarly());
    }

    // Prose that says its opening again is not a loop, and the stream must not
    // be cut on it. The detector used to fire on the first recurrence of the
    // opening with no further check at all, so this arrived truncated to the
    // first paragraph - the response-side collapse was the only one of the two
    // that ever verified the text was really tiled by that unit. They are now
    // the same code.
    void testStreamingDoesNotAbortOnProseThatEchoesItsOpening()
    {
        const QString straight = QStringLiteral(
            "His music can be found in the award winning indie games Actual Sunlight, "
            "This Is The Police I & II, and Hacknet: Labyrinths, HBO's Vice Principals");
        const QString echoed = straight + QStringLiteral("\nHe also scored several short films.\n") + straight;

        MockHttpServer server;
        Response response;
        response.status = 200;
        QList<QByteArray> chunks;
        for (int at = 0; at < echoed.size(); at += 16) {
            QJsonObject delta;
            delta.insert(QStringLiteral("content"), echoed.mid(at, 16));
            QJsonObject choice;
            choice.insert(QStringLiteral("delta"), delta);
            QJsonObject body;
            body.insert(QStringLiteral("choices"), QJsonArray{choice});
            chunks.append(QByteArrayLiteral("data: ") + QJsonDocument(body).toJson(QJsonDocument::Compact) + QByteArrayLiteral("\n\n"));
        }
        chunks.append(QByteArrayLiteral("data: [DONE]\n\n"));
        response.streamChunks = chunks;
        response.chunkDelayMs = 2;
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("vision-model"));
        QSignalSpy recognizedSpy(&ocr, &AOcrProvider::recognized);

        QImage image(32, 32, QImage::Format_RGB32);
        image.fill(Qt::white);
        ocr.recognize(image, 96);

        QVERIFY(recognizedSpy.wait(10000));
        QCOMPARE(recognizedSpy.constFirst().at(0).toString(), echoed);
    }

    // A stream that never repeats must arrive whole.
    void testStreamingDeliversASingleTranscription()
    {
        const QString line = QStringLiteral("System Monitor\nCPU 12% Memory 1.2GiB\nNotifications are enabled");

        MockHttpServer server;
        Response response;
        response.status = 200;
        QList<QByteArray> chunks;
        for (int at = 0; at < line.size(); at += 16) {
            QJsonObject delta;
            delta.insert(QStringLiteral("content"), line.mid(at, 16));
            QJsonObject choice;
            choice.insert(QStringLiteral("delta"), delta);
            QJsonObject body;
            body.insert(QStringLiteral("choices"), QJsonArray{choice});
            chunks.append(QByteArrayLiteral("data: ") + QJsonDocument(body).toJson(QJsonDocument::Compact) + QByteArrayLiteral("\n\n"));
        }
        chunks.append(QByteArrayLiteral("data: [DONE]\n\n"));
        response.streamChunks = chunks;
        response.chunkDelayMs = 2;
        server.queueResponse(response);

        LlmOcr ocr;
        ocr.setEndpoint(server.baseUrl(), false, QString());
        ocr.setModel(QStringLiteral("vision-model"));
        QSignalSpy recognizedSpy(&ocr, &AOcrProvider::recognized);

        QImage image(32, 32, QImage::Format_RGB32);
        image.fill(Qt::white);
        ocr.recognize(image, 96);

        QVERIFY(recognizedSpy.wait(10000));
        QCOMPARE(recognizedSpy.constFirst().at(0).toString(), line);
    }

    // "returned an empty response" told the user nothing: it is the same
    // message whether the model reasoned away its budget, ran out of context,
    // or the server complained. Each case must now name itself, and the
    // reasoning one must name the setting that fixes it.
    void testEmptyResponseSaysWhy()
    {
        auto reasonFor = [](const QByteArray &body) {
            MockHttpServer server;
            Response response;
            response.status = 200;
            response.body = body;
            server.queueResponse(response);

            LlmOcr ocr;
            ocr.setEndpoint(server.baseUrl(), false, QString());
            ocr.setModel(QStringLiteral("vision-model"));
            QSignalSpy failedSpy(&ocr, &AOcrProvider::failed);
            QImage image(32, 32, QImage::Format_RGB32);
            image.fill(Qt::white);
            ocr.recognize(image, 96);
            if (!failedSpy.wait(5000))
                return QString();
            return failedSpy.constFirst().at(0).toString();
        };

        // Budget spent reasoning: must point at "Disable reasoning".
        const QString reasoned = reasonFor(QByteArrayLiteral(
            R"({"choices":[{"finish_reason":"length","message":{"content":"","reasoning":"thinking out loud"}}],)"
            R"("usage":{"prompt_tokens":3380,"completion_tokens":716,"total_tokens":4096}})"));
        QVERIFY(reasoned.contains(QStringLiteral("Disable reasoning")));

        // Out of context without reasoning: must report the token split.
        const QString exhausted = reasonFor(QByteArrayLiteral(
            R"({"choices":[{"finish_reason":"length","message":{"content":""}}],)"
            R"("usage":{"prompt_tokens":3900,"completion_tokens":196,"total_tokens":4096}})"));
        QVERIFY(exhausted.contains(QStringLiteral("3900")));
        QVERIFY(exhausted.contains(QStringLiteral("4096")));
        QVERIFY(!exhausted.contains(QStringLiteral("Disable reasoning")));

        // The provider's own message beats any transport-level string.
        const QString serverSaid = reasonFor(QByteArrayLiteral(
            R"({"error":{"message":"model 'nope' not found"}})"));
        QVERIFY(serverSaid.contains(QStringLiteral("model 'nope' not found")));
    }

    void testCollapseRepeatedTranscription()
    {
        // The repeat-prone-local-model signature: the completed transcription
        // re-emitted, blank-line separated. Collapses to one copy.
        const QString block = QStringLiteral("System Monitor\nCPU 12% Memory 1.2GiB\nNotifications are enabled");
        const QString repeated = block + QStringLiteral("\n\n") + block + QStringLiteral("\n\n") + block;
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(repeated), block);

        // A single clean transcription is left untouched.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(block), block);

        // Empty / whitespace-only trims to empty.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(QString()), QString());
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(QStringLiteral(" ")), QString());

        // Text too short to tell a loop from a repeated phrase stays put.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(QStringLiteral("no\nno\nno\nno")),
                 QStringLiteral("no\nno\nno\nno"));

        // Two whole copies is the bar.
        const QString twoCopies = block + QStringLiteral("\n\n") + block;
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(twoCopies), block);

        // THE BUG THIS FUNCTION EXISTS FOR (reported 2026-08-18, third
        // recurrence): the copies are NOT identical. glm-ocr transcribes the
        // same apostrophe as ' in some copies and U+2019 in others, so byte
        // equality found no repetition at all and the user got the paragraph
        // 43 times. Folding the punctuation is what makes them comparable.
        const QString straight = QStringLiteral(
            "His music can be found in the award winning indie games Actual Sunlight, "
            "This Is The Police I & II, and Hacknet: Labyrinths, HBO's Vice Principals");
        const QString curly = QString(straight).replace(QLatin1Char('\''), QChar(0x2019));
        const QString drifting = curly + QStringLiteral("\n") + straight + QStringLiteral("\n")
            + straight + QStringLiteral("\n") + straight;
        // One copy survives, and it is the ORIGINAL first copy - typography intact.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(drifting), curly);

        // The copies need not be newline separated: a model that repeats
        // inside one long line was previously invisible to this function,
        // which split on newlines and gave up at fewer than two lines.
        const QString runOn = straight + QStringLiteral(" ") + straight + QStringLiteral(" ") + straight;
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(runOn), straight);

        // The token budget usually runs out mid-copy, leaving a partial tail
        // that is a prefix of the unit.
        const QString truncated = block + QStringLiteral("\n\n") + block + QStringLiteral("\n\nSystem Monitor\nCPU 12%");
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(truncated), block);

        // Prose that echoes an earlier passage at the end is ONE copy plus a
        // partial - real content, not a loop, and deleting it would be data
        // loss. Stays whole.
        const QString echoed = straight + QStringLiteral("\nHe also scored several short films.\n") + straight;
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(echoed), echoed);

        // Long prose with no repetition at all is untouched.
        const QString prose = straight + QStringLiteral(" He also scored a documentary about deep sea exploration.");
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(prose), prose);
    }

    // THE FOURTH RECURRENCE (reported 2026-08-19, with a screenshot of a
    // two-character Chinese sign). Reproduced against the real glm-ocr with
    // the request the app sends: 4026 completion tokens, 2007 copies of the
    // word - and the collapse handed back FOURTEEN of them.
    //
    // kMinUnitLength was being applied as a floor on the unit: the scan
    // skipped every recurrence of the opening closer than 40 folded
    // characters, so for a three-character unit the first offset it would
    // accept was 42 - fourteen copies - and it then faithfully collapsed 2007
    // copies down to that. Nothing about the script matters, any unit under
    // 40 characters behaved this way; CJK just makes it the normal case,
    // because 40 CJK characters is a whole paragraph. What settles it is how
    // MUCH repetition there is, not how long one copy is.
    void testCollapseRepeatedShortUnitTranscription()
    {
        const QString word = cjkWord();
        const QString newline = QStringLiteral("\n");

        // The reported response, in its exact shape: the first copy set off
        // by a blank line, then copies to the end of the token budget.
        const QString reported = word + QStringLiteral("\n\n") + repeatedCopies(word, 2006, newline);
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(reported), word);

        // Enough copies to be conclusive, and one short of it.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(repeatedCopies(word, 21, newline)), word);
        const QString twenty = repeatedCopies(word, 20, newline);
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(twenty), twenty);

        // CJK copies often have no separator between them at all ...
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(repeatedCopies(word, 40, QString())), word);
        // ... or an ideographic space, U+3000, which folds like any other.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(repeatedCopies(word, 30, QStringLiteral("\u3000"))), word);

        // A handful of copies is content, not a loop - a sign really can say
        // the same word five times - and so are eight identical prices or a
        // row of dot leaders. None of them may be thrown away.
        const QString five = repeatedCopies(word, 5, newline);
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(five), five);
        const QString prices = repeatedCopies(QStringLiteral("$5"), 8, newline);
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(prices), prices);
        const QString leaders = QString(30, QLatin1Char('.'));
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(leaders), leaders);

        // The CJK form of the drift that defeated byte-equality dedup: the
        // model alternates between fullwidth punctuation and its halfwidth
        // twin from copy to copy, here an ideographic full stop U+3002 against
        // an ASCII one. ("Please submit the report before three o'clock.")
        const QString sentence = QStringLiteral("\u8bf7\u5728\u4e0b\u5348\u4e09\u70b9\u524d\u63d0\u4ea4\u62a5\u544a\u3002");
        const QString halfwidth = QString(sentence).replace(QChar(0x3002), QLatin1Char('.'));
        QStringList drifting;
        for (int copy = 0; copy < 10; ++copy) {
            drifting.append(copy % 2 == 0 ? sentence : halfwidth);
        }
        // One copy survives, and it is the ORIGINAL first copy - the
        // fullwidth punctuation the image actually had.
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(drifting.join(newline)), sentence);

        // A real CJK paragraph is longer than the old unit floor ever was, so
        // two copies still settle it. ("It is fine weather today. The meeting
        // is in the second meeting room from three in the afternoon. The
        // materials have been distributed in advance.")
        const QString paragraph = QStringLiteral("\u672c\u65e5\u306f\u6674\u5929\u306a\u308a\u3002")
            + QStringLiteral("\u4f1a\u8b70\u306f\u5348\u5f8c\u4e09\u6642\u304b\u3089\u7b2c\u4e8c\u4f1a\u8b70\u5ba4\u3067\u884c\u3044\u307e\u3059\u3002")
            + QStringLiteral("\u8cc7\u6599\u306f\u4e8b\u524d\u306b\u914d\u5e03\u6e08\u307f\u3067\u3059\u3002");
        QCOMPARE(LlmOcr::collapseRepeatedTranscription(paragraph + newline + paragraph), paragraph);
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