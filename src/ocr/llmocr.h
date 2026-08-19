/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LLMOCR_H
#define LLMOCR_H

#include "aocrprovider.h"

#include <QByteArray>
#include <QImage>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// OCR engine backed by an OpenAI-compatible or Anthropic-compatible vision
// model. Unlike the Tesseract engine it knows nothing about translation: it
// sends the image to a multimodal chat completions endpoint with a
// transcription prompt and emits the recognized text, which then flows
// through the same source-edit -> translation path as any other OCR result.
namespace OpenAiEndpoint
{
struct Completion;
}

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
    // re-emits until it exhausts its token budget, so a runaway repeat can
    // never land doubled in the source edit even when generation isn't
    // stopped upstream. Compares on a folded view of the text (typographic
    // and fullwidth punctuation normalised, whitespace collapsed) because the
    // copies are NOT byte-identical - glm-ocr alternates between ' and U+2019
    // for the same apostrophe - and returns a slice of the original, so the
    // real punctuation survives. Static so the unit test can pin it directly.
    static QString collapseRepeatedTranscription(const QString &text);

private:
    void onFinished();
    void onStreamData();
    // Index just past the first copy when `text` is nothing but repetitions of
    // its own opening, or -1. The streaming and response-side paths are the
    // same code: collapseRepeatedTranscription() is this plus a left().
    static int repeatOffset(const QString &text);
    // Turns a response that carried no text into a sentence that says which
    // of the several possible reasons it was.
    static QString emptyResponseReason(const OpenAiEndpoint::Completion &completion);

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
    bool m_stoppedOnRepeat = false;
    QByteArray m_streamBuffer;
    QByteArray m_rawBody;
    QString m_streamText;
};

#endif // LLMOCR_H
