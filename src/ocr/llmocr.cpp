/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "llmocr.h"

#include "llm/openaiendpoint.h"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

LlmOcr::LlmOcr(QObject *parent)
    : AOcrProvider(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

LlmOcr::~LlmOcr()
{
    if (m_reply != nullptr) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

QString LlmOcr::engineName() const
{
    return QStringLiteral("llm");
}

bool LlmOcr::isConfigured() const
{
    return !m_url.isEmpty() && !m_model.isEmpty();
}

void LlmOcr::setEndpoint(const QString &url, bool isAnthropic, const QString &apiKey)
{
    m_url = url;
    m_isAnthropic = isAnthropic;
    m_apiKey = apiKey;
}

void LlmOcr::setModel(const QString &model)
{
    m_model = model;
}

void LlmOcr::setTimeout(int seconds)
{
    m_timeout = seconds;
}

void LlmOcr::setPrompt(const QString &prompt)
{
    m_prompt = prompt;
}

void LlmOcr::setDisableThinking(bool disable)
{
    m_disableThinking = disable;
}

QString LlmOcr::defaultPrompt()
{
    return QStringLiteral(
        "Transcribe ALL text visible in this image, exactly as written, in reading order. "
        "Preserve the original language, line breaks and paragraphs. "
        "Output only the transcribed text, nothing else. If there is no text, output nothing.");
}

QString LlmOcr::collapseRepeatedTranscription(const QString &text)
{
    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    // Collapse runs of blank lines to a single newline so verbatim re-emissions
    // of the same block become a detectable repeating sequence of lines.
    QString folded;
    folded.reserve(normalized.size());
    const QChar newline = QLatin1Char('\n');
    int newlineRun = 0;
    for (const QChar c : normalized) {
        if (c == newline) {
            if (newlineRun == 0) {
                folded.append(newline);
            }
            ++newlineRun;
        } else {
            folded.append(c);
            newlineRun = 0;
        }
    }

    QStringList lines;
    const QStringList parts = folded.split(newline, Qt::SkipEmptyParts);
    lines.reserve(parts.size());
    for (const QString &part : parts) {
        const QString line = part.trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }

    const int n = lines.size();
    if (n < 2) {
        return text.trimmed();
    }

    // A whole-response repetition is the pathological signature: the completed
    // transcription, then the same block re-emitted verbatim until the token
    // budget runs out - which typically cuts the FINAL copy mid-block, leaving
    // a partial tail whose last line is a prefix of the block's next line.
    // Any such tiling is collapsed to a single unit, with no exception for
    // text that legitimately repeats: a scanned poem whose lines tile exactly
    // loses its repeats, and the reader can restore them (user decision
    // 2026-08-18). Trying to tell a refrain from a decoding loop by copy count
    // or line length only made the real failures depend on how many copies the
    // model happened to emit.
    for (int period = 1; period <= n / 2; ++period) {
        const int copies = n / period;
        if (copies < 2) {
            continue;
        }
        bool tiled = true;
        for (int i = period; i < n; ++i) {
            const QString &expected = lines[i % period];
            const bool matches = (i == n - 1) ? expected.startsWith(lines[i]) : (lines[i] == expected);
            if (!matches) {
                tiled = false;
                break;
            }
        }
        if (!tiled) {
            continue;
        }
        return lines.mid(0, period).join(newline);
    }
    return text.trimmed();
}

void LlmOcr::recognize(const QImage &image, int dpi)
{
    Q_UNUSED(dpi)

    emit started();

    if (m_reply != nullptr) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_userCanceled = false;

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG");
    const QString base64Data = QString::fromLatin1(imageData.toBase64());

    const QString prompt = m_prompt.isEmpty() ? defaultPrompt() : m_prompt;

    QJsonObject textPart;
    textPart.insert(QStringLiteral("type"), QStringLiteral("text"));
    textPart.insert(QStringLiteral("text"), prompt);

    QJsonObject imagePart;
    if (m_isAnthropic) {
        imagePart.insert(QStringLiteral("type"), QStringLiteral("image"));
        QJsonObject source;
        source.insert(QStringLiteral("type"), QStringLiteral("base64"));
        source.insert(QStringLiteral("media_type"), QStringLiteral("image/jpeg"));
        source.insert(QStringLiteral("data"), base64Data);
        imagePart.insert(QStringLiteral("source"), source);
    } else {
        imagePart.insert(QStringLiteral("type"), QStringLiteral("image_url"));
        QJsonObject imageUrl;
        imageUrl.insert(QStringLiteral("url"), QStringLiteral("data:image/jpeg;base64,") + base64Data);
        imagePart.insert(QStringLiteral("image_url"), imageUrl);
    }

    QJsonArray content;
    content.append(textPart);
    content.append(imagePart);

    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("user"));
    message.insert(QStringLiteral("content"), content);

    QJsonArray messages;
    messages.append(message);

    // Small local vision models (an Ollama-hosted OCR model is the case that
    // surfaced this) reliably finish the real transcription, then - lacking a
    // clean stop token for the task - wrap it in a markdown fence and loop
    // re-emitting it verbatim until they hit the token budget. A stop
    // sequence on the fence marker cuts generation the instant that starts,
    // instead of shipping the runaway repeat to the caller and trying to
    // clean it up after the fact. Plain transcriptions never emit "```"
    // themselves (the prompt asks for the transcribed text and nothing else).
    static const QJsonArray kFenceStopSequence = {QStringLiteral("```")};

    QJsonObject body;
    body.insert(QStringLiteral("model"), m_model);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("temperature"), 0.0);
    if (m_isAnthropic) {
        body.insert(QStringLiteral("max_tokens"), 4096);
        body.insert(QStringLiteral("stop_sequences"), kFenceStopSequence);
    } else {
        body.insert(QStringLiteral("stream"), false);
        body.insert(QStringLiteral("stop"), kFenceStopSequence);
        if (m_disableThinking) {
            body.insert(QStringLiteral("reasoning_effort"), QStringLiteral("none"));
        }
    }

    QNetworkRequest request(QUrl(OpenAiEndpoint::completionsUrl(m_url, m_isAnthropic)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    OpenAiEndpoint::setAuthHeaders(request, m_isAnthropic, m_apiKey);
    m_network->setTransferTimeout(m_timeout * 1000);

    m_reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, &LlmOcr::onFinished);
}

void LlmOcr::cancel()
{
    if (m_reply != nullptr) {
        m_userCanceled = true;
        m_reply->abort();
    } else {
        emit canceled();
    }
}

void LlmOcr::onFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr) {
        return;
    }
    if (reply != m_reply) {
        reply->deleteLater();
        return;
    }
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        if (m_userCanceled) {
            emit canceled();
        } else {
            emit failed(tr("LLM OCR request timed out"));
        }
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(tr("LLM OCR error: %1").arg(reply->errorString()));
        return;
    }

    const QString text = OpenAiEndpoint::extractContent(reply->readAll(), m_isAnthropic);
    if (text.isEmpty()) {
        emit failed(tr("LLM OCR returned an empty response"));
        return;
    }

    emit recognized(collapseRepeatedTranscription(text));
}
