/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef VISIONMODELPROBE_H
#define VISIONMODELPROBE_H

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

// Asks a server what models it has and, where the server is willing to say,
// which of them accept images.
//
// Only two of the supported server kinds report capabilities at all: Ollama
// inlines a "capabilities" array in /api/tags, and LM Studio reports a model
// type/vision flag in its native /api/v0/models. The OpenAI-compatible
// /v1/models response - which is all Anthropic, FastFlowLM and arbitrary
// cloud endpoints offer - carries no capability information whatsoever.
//
// So "not in visionModels" means "this server did not tell us", never "this
// model cannot do vision". Callers must present the two lists as
// proven/unproven and must not filter the unproven ones away: a correct
// vision model behind a plain /v1/models endpoint would vanish from the UI.
class VisionModelProbe : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(VisionModelProbe)

public:
    explicit VisionModelProbe(QObject *parent = nullptr);
    ~VisionModelProbe() override;

    // providerId selects the probing strategy; baseUrl is whatever the user
    // typed and is normalized through OpenAiEndpoint.
    void probe(const QString &providerId, const QString &baseUrl, const QString &apiKey);
    void cancel();

    // True when the provider kind can report vision capability at all, i.e.
    // when an empty visionModels list is meaningful rather than merely
    // uninformative.
    static bool reportsCapabilities(const QString &providerId);

signals:
    // allModels is everything the server listed, in server order.
    // visionModels is the subset it explicitly marked as image-capable.
    void finished(const QStringList &allModels, const QStringList &visionModels);
    void failed(const QString &error);

private:
    void requestOpenAiModels(const QString &baseUrl, const QString &apiKey);
    void onNativeFinished();
    void onOpenAiFinished();

    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QString m_providerId;
    QString m_baseUrl;
    QString m_apiKey;
};

#endif // VISIONMODELPROBE_H
