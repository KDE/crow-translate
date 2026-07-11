/*
 * SPDX-FileCopyrightText: 2026 Oleksandr Mikriukov <ur3ley@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LOCALAITRANSLATIONPROVIDER_H
#define LOCALAITRANSLATIONPROVIDER_H

#include "language.h"
#include "translator/atranslationprovider.h"

#include <QObject>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

// Private translation backend for local, OpenAI-compatible servers
// (Ollama, FastFlowLM, LM Studio). Talks /v1/chat/completions and /v1/models.
// Translation runs on the active provider; language detection can use a
// different provider/model. No proxy, no cache of text, no request logging.
class LocalAiTranslationProvider : public ATranslationProvider
{
    Q_OBJECT
    Q_DISABLE_COPY(LocalAiTranslationProvider)

public:
    explicit LocalAiTranslationProvider(QObject *parent = nullptr);
    ~LocalAiTranslationProvider() override;

    QString getProviderType() const override;

    QVector<Language> supportedSourceLanguages() override;
    QVector<Language> supportedDestinationLanguages() override;
    bool supportsAutodetection() const override;
    Language detectLanguage(const QString &text) override;
    void abort() override;

    void setSourceImage(const QByteArray &imageData) override;
    void clearSourceImage() override;
    bool hasSourceImage() const override;

    void applyOptions(const ProviderOptions &options) override;
    std::unique_ptr<ProviderOptions> getDefaultOptions() const override;
    QStringList getAvailableOptions() const override;

    ProviderUIRequirements getUIRequirements() const override;

    void saveOptionToSettings(const QString &optionKey, const QVariant &value) override;

public slots:
    void translate(const QString &inputText, const Language &translationLanguage, const Language &sourceLanguage) override;

private slots:
    void onReplyFinished();
    void onDetectFinished();

private:
    QString buildPrompt(const QString &srcCode, const QString &dstCode, const QString &text) const;
    static QString formatResult(const QString &text);
    static QString chatUrl(const QString &baseUrl);
    void sendDetection(const QString &text);
    void sendTranslation(const QString &srcCode, const QString &dstCode, const QString &text);

    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply;
    QNetworkReply *m_detectReply;

    // Translation (active provider)
    QString m_url;
    QString m_model;
    QString m_prompt;
    bool m_disableThinking = false;

    // Detection (independent provider/model)
    bool m_detectViaLlm;
    QString m_detectUrl;
    QString m_detectModel;

    // In-flight state
    bool m_sourceWasAuto;
    bool m_detectThenTranslate;
    bool m_userAborted = false;
    QString m_pendingText;
    QString m_pendingDstCode;

    int m_timeout = 300;

    // Vision
    bool m_visionEnabled = false;
    QString m_visionModel;
    QString m_visionPrompt;
    bool m_visionDisableThinking = false;
    int m_visionTimeout = 300;
    QByteArray m_imageData;
};

#endif // LOCALAITRANSLATIONPROVIDER_H
