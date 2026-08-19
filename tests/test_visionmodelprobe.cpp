/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// VisionModelProbe drives the OCR settings page's "Refresh models" button.
// It has to cope with three different model-list wire shapes and degrade
// cleanly when the capability-reporting endpoint is missing, so every one of
// those branches is pinned here against a local server rather than a live
// Ollama (tests/test_llmocr_live.cpp is the live counterpart).

#include "mockhttpserver.h"
#include "llm/visionmodelprobe.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using Response = MockHttpServer::Response;

namespace
{

QJsonObject namedModel(const QString &key, const QString &name)
{
    QJsonObject model;
    model.insert(key, name);
    return model;
}

QByteArray wrap(const QString &key, const QJsonArray &models)
{
    QJsonObject root;
    root.insert(key, models);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void queueJson(MockHttpServer &server, const QByteArray &body)
{
    Response response;
    response.status = 200;
    response.body = body;
    server.queueResponse(response);
}

} // namespace

class VisionModelProbeTest : public QObject
{
    Q_OBJECT

private slots:
    // Only the two providers with a documented native capability endpoint may
    // take the native path; everything else must go straight to /v1/models.
    void testReportsCapabilitiesOnlyForOllamaAndLmStudio()
    {
        QVERIFY(VisionModelProbe::reportsCapabilities(QStringLiteral("ollama")));
        QVERIFY(VisionModelProbe::reportsCapabilities(QStringLiteral("lmstudio")));
        QVERIFY(!VisionModelProbe::reportsCapabilities(QStringLiteral("openai")));
        QVERIFY(!VisionModelProbe::reportsCapabilities(QStringLiteral("anthropic")));
        QVERIFY(!VisionModelProbe::reportsCapabilities(QString()));
    }

    // Ollama: GET /api/tags, array keyed "models", name in "name", vision
    // advertised through the "capabilities" array.
    void testOllamaNativeShapeReportsVisionFromCapabilities()
    {
        MockHttpServer server;
        QJsonArray models;
        QJsonObject vision = namedModel(QStringLiteral("name"), QStringLiteral("llava:7b"));
        vision.insert(QStringLiteral("capabilities"), QJsonArray{QStringLiteral("completion"), QStringLiteral("vision")});
        models.append(vision);
        models.append(namedModel(QStringLiteral("name"), QStringLiteral("llama3:8b")));
        queueJson(server, wrap(QStringLiteral("models"), models));

        VisionModelProbe probe;
        QSignalSpy finishedSpy(&probe, &VisionModelProbe::finished);
        probe.probe(QStringLiteral("ollama"), server.baseUrl(), QString());
        QVERIFY(finishedSpy.wait(5000));

        QCOMPARE(server.requestPath(0), QStringLiteral("/api/tags"));
        QCOMPARE(finishedSpy.constFirst().at(0).toStringList(), QStringList({QStringLiteral("llava:7b"), QStringLiteral("llama3:8b")}));
        QCOMPARE(finishedSpy.constFirst().at(1).toStringList(), QStringList({QStringLiteral("llava:7b")}));
    }

    // LM Studio: GET /api/v0/models, array keyed "data", and it types its
    // vision-language models "vlm" instead of setting a flag.
    void testLmStudioNativeShapeReportsVisionFromTypeAndFlag()
    {
        MockHttpServer server;
        QJsonArray models;
        QJsonObject vlm = namedModel(QStringLiteral("id"), QStringLiteral("qwen2-vl"));
        vlm.insert(QStringLiteral("type"), QStringLiteral("vlm"));
        models.append(vlm);
        QJsonObject flagged = namedModel(QStringLiteral("id"), QStringLiteral("gemma-vision"));
        flagged.insert(QStringLiteral("vision"), true);
        models.append(flagged);
        QJsonObject plain = namedModel(QStringLiteral("id"), QStringLiteral("mistral-7b"));
        plain.insert(QStringLiteral("type"), QStringLiteral("llm"));
        models.append(plain);
        queueJson(server, wrap(QStringLiteral("data"), models));

        VisionModelProbe probe;
        QSignalSpy finishedSpy(&probe, &VisionModelProbe::finished);
        probe.probe(QStringLiteral("lmstudio"), server.baseUrl(), QString());
        QVERIFY(finishedSpy.wait(5000));

        QCOMPARE(server.requestPath(0), QStringLiteral("/api/v0/models"));
        QCOMPARE(finishedSpy.constFirst().at(1).toStringList(),
                 QStringList({QStringLiteral("qwen2-vl"), QStringLiteral("gemma-vision")}));
    }

    // A provider with no native endpoint must not probe one at all.
    void testNonCapabilityProviderGoesStraightToOpenAiModels()
    {
        MockHttpServer server;
        QJsonArray models;
        models.append(namedModel(QStringLiteral("id"), QStringLiteral("gpt-4o")));
        queueJson(server, wrap(QStringLiteral("data"), models));

        VisionModelProbe probe;
        QSignalSpy finishedSpy(&probe, &VisionModelProbe::finished);
        probe.probe(QStringLiteral("openai"), server.baseUrl(), QString());
        QVERIFY(finishedSpy.wait(5000));

        QCOMPARE(server.requestCount(), 1);
        QVERIFY(server.requestPath(0).endsWith(QStringLiteral("/models")));
        QVERIFY(!server.requestPath(0).contains(QStringLiteral("/api/")));
        QCOMPARE(finishedSpy.constFirst().at(0).toStringList(), QStringList({QStringLiteral("gpt-4o")}));
        QVERIFY(finishedSpy.constFirst().at(1).toStringList().isEmpty());
    }

    // An Ollama behind a proxy that only forwards /v1 still answers
    // /v1/models: a 404 on the native endpoint must degrade to the plain
    // list, not fail the whole refresh.
    void testNativeEndpointErrorFallsBackToOpenAiModels()
    {
        MockHttpServer server;
        Response notFound;
        notFound.status = 404;
        notFound.body = QByteArrayLiteral("{}");
        server.queueResponse(notFound);
        QJsonArray models;
        models.append(namedModel(QStringLiteral("id"), QStringLiteral("llama3:8b")));
        queueJson(server, wrap(QStringLiteral("data"), models));

        VisionModelProbe probe;
        QSignalSpy finishedSpy(&probe, &VisionModelProbe::finished);
        QSignalSpy failedSpy(&probe, &VisionModelProbe::failed);
        probe.probe(QStringLiteral("ollama"), server.baseUrl(), QString());
        QVERIFY(finishedSpy.wait(5000));

        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(server.requestCount(), 2);
        QCOMPARE(server.requestPath(0), QStringLiteral("/api/tags"));
        QVERIFY(server.requestPath(1).endsWith(QStringLiteral("/models")));
        QCOMPARE(finishedSpy.constFirst().at(0).toStringList(), QStringList({QStringLiteral("llama3:8b")}));
    }

    // A 200 that parses to no usable names is just as useless as a 404 -
    // same fallback, so a newer LM Studio that moved its native API still
    // populates the combo.
    void testNativeEndpointWithNoUsableNamesFallsBackToOpenAiModels()
    {
        MockHttpServer server;
        queueJson(server, wrap(QStringLiteral("models"), QJsonArray{QJsonObject{}}));
        QJsonArray models;
        models.append(namedModel(QStringLiteral("id"), QStringLiteral("recovered-model")));
        queueJson(server, wrap(QStringLiteral("data"), models));

        VisionModelProbe probe;
        QSignalSpy finishedSpy(&probe, &VisionModelProbe::finished);
        probe.probe(QStringLiteral("ollama"), server.baseUrl(), QString());
        QVERIFY(finishedSpy.wait(5000));

        QCOMPARE(server.requestCount(), 2);
        QCOMPARE(finishedSpy.constFirst().at(0).toStringList(), QStringList({QStringLiteral("recovered-model")}));
    }

    // Name resolution order is name -> model -> id; entries with none of the
    // three are dropped rather than appearing as blank combo rows.
    void testModelNameFallsBackThroughNameModelAndId()
    {
        MockHttpServer server;
        QJsonArray models;
        models.append(namedModel(QStringLiteral("name"), QStringLiteral("from-name")));
        models.append(namedModel(QStringLiteral("model"), QStringLiteral("from-model")));
        models.append(namedModel(QStringLiteral("id"), QStringLiteral("from-id")));
        models.append(QJsonObject{});
        queueJson(server, wrap(QStringLiteral("models"), models));

        VisionModelProbe probe;
        QSignalSpy finishedSpy(&probe, &VisionModelProbe::finished);
        probe.probe(QStringLiteral("ollama"), server.baseUrl(), QString());
        QVERIFY(finishedSpy.wait(5000));

        QCOMPARE(finishedSpy.constFirst().at(0).toStringList(),
                 QStringList({QStringLiteral("from-name"), QStringLiteral("from-model"), QStringLiteral("from-id")}));
    }

    // The OpenAI path is the last resort, so its failure is the one the user
    // must actually be told about.
    void testOpenAiPathErrorEmitsFailed()
    {
        MockHttpServer server;
        Response serverError;
        serverError.status = 500;
        serverError.body = QByteArrayLiteral("{}");
        server.queueResponse(serverError);

        VisionModelProbe probe;
        QSignalSpy finishedSpy(&probe, &VisionModelProbe::finished);
        QSignalSpy failedSpy(&probe, &VisionModelProbe::failed);
        probe.probe(QStringLiteral("openai"), server.baseUrl(), QString());
        QVERIFY(failedSpy.wait(5000));

        QCOMPARE(finishedSpy.count(), 0);
        QVERIFY(!failedSpy.constFirst().at(0).toString().isEmpty());
    }

    // Cancelling a probe in flight must stay silent - the settings dialog
    // cancels when the user switches provider, and a late finished() would
    // repopulate the combo for the wrong endpoint.
    void testCancelledProbeEmitsNothing()
    {
        MockHttpServer server;
        Response held;
        held.hang = true;
        server.queueResponse(held);

        VisionModelProbe probe;
        QSignalSpy finishedSpy(&probe, &VisionModelProbe::finished);
        QSignalSpy failedSpy(&probe, &VisionModelProbe::failed);
        probe.probe(QStringLiteral("openai"), server.baseUrl(), QString());
        QTRY_COMPARE(server.requestCount(), 1);

        probe.cancel();
        server.releaseHeldRequest(0);
        QTest::qWait(300);

        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(failedSpy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(VisionModelProbeTest)

#include "test_visionmodelprobe.moc"
