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

// Translation backend for local, OpenAI-compatible servers (Ollama,
// FastFlowLM, LM Studio) as well as arbitrary remote endpoints: a custom
// OpenAI-compatible host/cloud API, or Anthropic. Local ids talk
// /v1/chat/completions + /v1/models; the "anthropic" id talks
// /v1/messages + /v1/models with Anthropic's request/response shape
// instead. Translation runs on the active provider; language detection can
// use a different provider/model. No proxy, no cache of text, no request
// logging.
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

    void applyOptions(const ProviderOptions &options) override;
    std::unique_ptr<ProviderOptions> getDefaultOptions() const override;
    QStringList getAvailableOptions() const override;

    ProviderCapabilities capabilities() const override;

    void saveOptionToSettings(const QString &optionKey, const QVariant &value) override;

public slots:
    void translate(const QString &inputText, const Language &translationLanguage, const Language &sourceLanguage) override;

private slots:
    void onReplyFinished();
    void onDetectFinished();

private:
    QString buildPrompt(const QString &srcCode, const QString &dstCode, const QString &text) const;
    static QString languageDisplayName(const QString &code);
    static TranslationResult formatResult(const QString &text);
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
    bool m_isAnthropic = false;
    QString m_apiKey;

    // Detection (independent provider/model)
    QString m_detectUrl;
    QString m_detectModel;
    bool m_detectIsAnthropic = false;
    QString m_detectApiKey;

    // In-flight state
    bool m_sourceWasAuto;
    bool m_detectThenTranslate;
    bool m_userAborted = false;
    QString m_pendingText;
    QString m_pendingDstCode;

    int m_timeout = 300;
};

#endif // LOCALAITRANSLATIONPROVIDER_H
