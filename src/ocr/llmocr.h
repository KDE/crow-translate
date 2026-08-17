/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LLMOCR_H
#define LLMOCR_H

#include "aocrprovider.h"

#include <QImage>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// OCR engine backed by an OpenAI-compatible or Anthropic-compatible vision
// model. Unlike the Tesseract engine it knows nothing about translation: it
// sends the image to a multimodal chat completions endpoint with a
// transcription prompt and emits the recognized text, which then flows
// through the same source-edit -> translation path as any other OCR result.
class LlmOcr : public AOcrProvider
{
    Q_OBJECT
    Q_DISABLE_COPY(LlmOcr)

public:
    explicit LlmOcr(QObject *parent = nullptr);
    ~LlmOcr() override;

    QString engineName() const override;
    bool isConfigured() const override;
    void recognize(const QImage &image, int dpi) override;
    void cancel() override;

    void setEndpoint(const QString &url, bool isAnthropic, const QString &apiKey);
    void setModel(const QString &model);
    void setTimeout(int seconds);
    void setPrompt(const QString &prompt);
    void setDisableThinking(bool disable);

    static QString defaultPrompt();

    // Collapses a transcription that a repeat-prone local vision model
    // re-emits verbatim (blank-line separated) until it exhausts its token
    // budget, so a runaway repeat can never land doubled in the source edit
    // even when generation isn't stopped upstream. Static so the unit test
    // can pin it directly.
    static QString collapseRepeatedTranscription(const QString &text);

private:
    void onFinished();

    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;

    QString m_url;
    QString m_model;
    QString m_prompt;
    QString m_apiKey;
    bool m_isAnthropic = false;
    int m_timeout = 300;
    bool m_userCanceled = false;
    bool m_disableThinking = false;
};

#endif // LLMOCR_H
