/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "visionmodelprobe.h"

#include "openaiendpoint.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace
{

constexpr int kProbeTimeoutMs = 5000;

bool hasVisionCapability(const QJsonObject &model)
{
    // Ollama: "capabilities": ["completion", "vision", ...]
    const QJsonArray capabilities = model.value(QStringLiteral("capabilities")).toArray();
    for (const QJsonValue &capability : capabilities) {
        if (capability.toString().compare(QLatin1String("vision"), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    // LM Studio reports it as a flag, and types its vision-language models
    // "vlm" rather than "llm". Accept both spellings - its native API is not
    // versioned in a way worth betting a feature on.
    if (model.value(QStringLiteral("vision")).toBool()) {
        return true;
    }
    return model.value(QStringLiteral("type")).toString().compare(QLatin1String("vlm"), Qt::CaseInsensitive) == 0;
}

} // namespace

VisionModelProbe::VisionModelProbe(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setTransferTimeout(kProbeTimeoutMs);
}

VisionModelProbe::~VisionModelProbe()
{
    if (m_reply != nullptr) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

bool VisionModelProbe::reportsCapabilities(const QString &providerId)
{
    return providerId == QLatin1String("ollama") || providerId == QLatin1String("lmstudio");
}

void VisionModelProbe::cancel()
{
    if (m_reply != nullptr) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void VisionModelProbe::probe(const QString &providerId, const QString &baseUrl, const QString &apiKey)
{
    cancel();
    m_providerId = providerId;
    m_baseUrl = baseUrl;
    m_apiKey = apiKey;

    if (!reportsCapabilities(providerId)) {
        requestOpenAiModels(baseUrl, apiKey);
        return;
    }

    const QString root = OpenAiEndpoint::serverRoot(baseUrl);
    const QString nativeUrl = providerId == QLatin1String("ollama")
        ? root + QStringLiteral("/api/tags")
        : root + QStringLiteral("/api/v0/models");

    QNetworkRequest request{QUrl(nativeUrl)};
    OpenAiEndpoint::setAuthHeaders(request, false, apiKey);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &VisionModelProbe::onNativeFinished);
}

void VisionModelProbe::requestOpenAiModels(const QString &baseUrl, const QString &apiKey)
{
    QNetworkRequest request{QUrl(OpenAiEndpoint::modelsUrl(baseUrl))};
    OpenAiEndpoint::setAuthHeaders(request, m_providerId == QLatin1String("anthropic"), apiKey);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &VisionModelProbe::onOpenAiFinished);
}

void VisionModelProbe::onNativeFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr || reply != m_reply) {
        if (reply != nullptr) {
            reply->deleteLater();
        }
        return;
    }
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // The native endpoint is the only source of capability information,
        // but it is not the only way to list models: an Ollama behind a proxy
        // that only forwards /v1, or a newer LM Studio that moved its native
        // API, still answers /v1/models. Fall back to the list without
        // capabilities rather than failing the whole refresh.
        requestOpenAiModels(m_baseUrl, m_apiKey);
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    // Ollama keys the array "models", LM Studio keys it "data".
    QJsonArray models = root.value(QStringLiteral("models")).toArray();
    if (models.isEmpty()) {
        models = root.value(QStringLiteral("data")).toArray();
    }

    QStringList allModels;
    QStringList visionModels;
    for (const QJsonValue &value : std::as_const(models)) {
        const QJsonObject model = value.toObject();
        QString name = model.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = model.value(QStringLiteral("model")).toString();
        }
        if (name.isEmpty()) {
            name = model.value(QStringLiteral("id")).toString();
        }
        if (name.isEmpty()) {
            continue;
        }
        allModels.append(name);
        if (hasVisionCapability(model)) {
            visionModels.append(name);
        }
    }

    if (allModels.isEmpty()) {
        requestOpenAiModels(m_baseUrl, m_apiKey);
        return;
    }

    emit finished(allModels, visionModels);
}

void VisionModelProbe::onOpenAiFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr || reply != m_reply) {
        if (reply != nullptr) {
            reply->deleteLater();
        }
        return;
    }
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
        return;
    }

    const QJsonArray data = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("data")).toArray();

    QStringList allModels;
    QStringList visionModels;
    for (const QJsonValue &value : std::as_const(data)) {
        const QJsonObject model = value.toObject();
        const QString name = model.value(QStringLiteral("id")).toString();
        if (name.isEmpty()) {
            continue;
        }
        allModels.append(name);
        // Almost never present on a /v1/models response, but an OpenAI-
        // compatible server that does report it should be believed.
        if (hasVisionCapability(model)) {
            visionModels.append(name);
        }
    }

    emit finished(allModels, visionModels);
}
