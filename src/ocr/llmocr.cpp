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
#include <QStringView>

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

namespace
{
// Comparison-only view of the text: the model re-emits its transcription with
// small sampling drift, so byte equality is the wrong primitive. Folding the
// characters it is inconsistent about - typographic quotes and dashes, and
// every flavour of whitespace - makes the copies comparable. A parallel index
// map records where each folded character came from, so the caller can slice
// the ORIGINAL string and keep its real punctuation.
struct FoldedText {
    QString text;
    QList<int> sourceIndex;
};

QChar foldChar(QChar c)
{
    const char16_t u = c.unicode();
    // A CJK transcription alternates between the fullwidth forms of ASCII and
    // their halfwidth twins the same way an English one alternates between '
    // and U+2019. One character in, one character out, so the index map that
    // maps the folded view back onto the original stays aligned.
    if (u >= 0xFF01 && u <= 0xFF5E) {
        return QChar(static_cast<char16_t>(u - 0xFEE0));
    }
    switch (u) {
    case 0x2018: // ' left single quote
    case 0x2019: // ' right single quote / apostrophe
    case 0x02BC: // modifier letter apostrophe
        return QLatin1Char('\'');
    case 0x201C: // " left double quote
    case 0x201D: // " right double quote
    case 0x300C: // left corner bracket
    case 0x300D: // right corner bracket
    case 0x300E: // left white corner bracket
    case 0x300F: // right white corner bracket
        return QLatin1Char('"');
    case 0x3001: // ideographic comma
        return QLatin1Char(',');
    case 0x3002: // ideographic full stop
        return QLatin1Char('.');
    case 0x2010: // hyphen
    case 0x2011: // non-breaking hyphen
    case 0x2012: // figure dash
    case 0x2013: // en dash
    case 0x2014: // em dash
        return QLatin1Char('-');
    default:
        return c;
    }
}

FoldedText foldForComparison(const QString &text)
{
    FoldedText folded;
    folded.text.reserve(text.size());
    folded.sourceIndex.reserve(text.size());

    bool pendingSpace = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar raw = text.at(i);
        if (raw.isSpace()) {
            pendingSpace = !folded.text.isEmpty();
            continue;
        }
        if (pendingSpace) {
            folded.text.append(QLatin1Char(' '));
            folded.sourceIndex.append(i);
            pendingSpace = false;
        }
        folded.text.append(foldChar(raw));
        folded.sourceIndex.append(i);
    }
    return folded;
}

// What makes a repeat conclusive is how much of it there is, not how long one
// copy is: a two-character unit said two hundred times is every bit as much a
// decoding loop as a paragraph said twice. So a long unit settles it after two
// copies - no screen region holds the same paragraph twice - while a short one
// has to fill the response before it means anything, because "no no no no" is
// a real thing to find in an image.
constexpr int kMinUnitLength = 40;
constexpr int kMinShortUnitCopies = 6;
constexpr int kMinShortUnitSpan = 60;
constexpr int kMaxProbeLength = 120;
// A trailing tile at least this fraction of a unit is a whole copy whose
// separator the whitespace folding absorbed, not a truncated one.
constexpr double kNearlyWholeTile = 0.9;
// Sampling drift means copies are not all identical; require the clear
// majority to match so unrelated text can never tile by accident.
constexpr double kMatchingTileRatio = 0.9;
// The opening can recur without the text being tiled by it. Trying every such
// place is quadratic, and this runs on every chunk of a stream.
constexpr int kMaxCandidateOffsets = 64;
// A transcription of a screen region never needs this many tokens; it exists
// so a runaway is bounded even if the repeat detector never fires.
constexpr int kMaxOutputTokens = 4096;

bool isConclusiveRepeat(int unitLength, int copies)
{
    if (copies < 2) {
        return false;
    }
    if (unitLength >= kMinUnitLength) {
        return true;
    }
    return copies >= kMinShortUnitCopies && unitLength * copies >= kMinShortUnitSpan;
}

// How many whole copies of its own opening `view` is made of, or -1 if that
// opening does not tile it at all.
int completeCopies(QStringView view, int unitLength)
{
    const int length = view.size();
    const QStringView unit = view.left(unitLength);

    int wholeTiles = 0;
    int matchingTiles = 0;
    for (int offset = 0; offset + unitLength <= length; offset += unitLength) {
        ++wholeTiles;
        if (view.sliced(offset, unitLength) == unit) {
            ++matchingTiles;
        }
    }
    if (wholeTiles == 0 || static_cast<double>(matchingTiles) / wholeTiles < kMatchingTileRatio) {
        return -1;
    }

    int copies = wholeTiles;
    const int tailLength = length % unitLength;
    if (tailLength > 0) {
        // The budget usually runs out mid-copy, leaving a tail that is a
        // prefix of the unit. Anything else means this is not a repetition.
        if (!unit.startsWith(view.sliced(length - tailLength, tailLength))) {
            return -1;
        }
        if (tailLength >= kNearlyWholeTile * unitLength) {
            ++copies;
        }
    }
    return copies;
}

// Length of one copy, in folded characters, when `view` is nothing but
// repetitions of its own opening - or -1 when it is ordinary text.
int repeatedUnitLength(QStringView view)
{
    const int length = view.size();
    if (length < kMinShortUnitSpan) {
        return -1;
    }

    // The pathology is the output starting to say itself again, so look for
    // exactly that: the point where the opening of the response recurs. That
    // offset IS the length of one copy - no need to try every divisor, and it
    // works whether or not the copies are separated by newlines. Half the text
    // is the longest probe that can still recur inside it.
    const QStringView probe = view.left(qMin(kMaxProbeLength, length / 2));

    int from = 1;
    for (int tried = 0; tried < kMaxCandidateOffsets; ++tried) {
        const int at = view.indexOf(probe, from);
        if (at < 0) {
            return -1;
        }
        const int copies = completeCopies(view, at);
        if (copies > 0 && isConclusiveRepeat(at, copies)) {
            return at;
        }
        from = at + 1;
    }
    return -1;
}

} // namespace

QString LlmOcr::emptyResponseReason(const OpenAiEndpoint::Completion &completion)
{
    const bool haveCounts = completion.promptTokens >= 0 && completion.totalTokens > 0;

    if (completion.ranOutOfBudget() && completion.spentBudgetReasoning()) {
        return tr("The model ran out of tokens before transcribing anything, having spent them reasoning instead. "
                  "Turn on \"Disable reasoning\" for this provider, or pick a model with a larger context.");
    }
    if (completion.ranOutOfBudget()) {
        if (haveCounts) {
            return tr("The model ran out of tokens before transcribing anything: the image used %1 of its %2 token "
                      "limit. Try a smaller region, or a model with a larger context.")
                .arg(completion.promptTokens)
                .arg(completion.totalTokens);
        }
        return tr("The model ran out of tokens before transcribing anything. Try a smaller region, or a model with a "
                  "larger context.");
    }
    if (!completion.errorMessage.isEmpty()) {
        return tr("LLM OCR error: %1").arg(completion.errorMessage);
    }
    return tr("The model returned no text for this image.");
}

QString LlmOcr::collapseRepeatedTranscription(const QString &text)
{
    const int offset = repeatOffset(text);
    return offset > 0 ? text.left(offset).trimmed() : text.trimmed();
}

// Index just past the first copy, in the ORIGINAL string, so what comes back
// keeps the real punctuation the folded view normalised away.
int LlmOcr::repeatOffset(const QString &text)
{
    const FoldedText folded = foldForComparison(text);
    const int unitLength = repeatedUnitLength(folded.text);
    if (unitLength < 0) {
        return -1;
    }

    // The unit ends with the separator that folding turned into a space, and
    // a folded space is indexed to the character that FOLLOWS it - which is
    // the next copy's first character. Walk back to the last real character
    // of this copy before mapping the offset onto the original string.
    int lastRealChar = unitLength - 1;
    while (lastRealChar > 0 && folded.text.at(lastRealChar) == QLatin1Char(' ')) {
        --lastRealChar;
    }
    return folded.sourceIndex.at(lastRealChar) + 1;
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
        body.insert(QStringLiteral("max_tokens"), kMaxOutputTokens);
        body.insert(QStringLiteral("stop_sequences"), kFenceStopSequence);
    } else {
        // Streamed, so a model that starts repeating itself can be cut off at
        // the moment it does rather than after it has burned its whole budget
        // - which for the reported screenshot meant 3876 tokens and 22
        // seconds spent producing 42 copies nobody wanted. max_tokens bounds
        // the damage if detection somehow never fires; it was missing here
        // entirely, so only the Anthropic branch was ever capped.
        body.insert(QStringLiteral("stream"), true);
        body.insert(QStringLiteral("max_tokens"), kMaxOutputTokens);
        body.insert(QStringLiteral("stop"), kFenceStopSequence);
        if (m_disableThinking) {
            body.insert(QStringLiteral("reasoning_effort"), QStringLiteral("none"));
        }
    }

    QNetworkRequest request(QUrl(OpenAiEndpoint::completionsUrl(m_url, m_isAnthropic)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    OpenAiEndpoint::setAuthHeaders(request, m_isAnthropic, m_apiKey);
    m_network->setTransferTimeout(m_timeout * 1000);

    m_streamBuffer.clear();
    m_streamText.clear();
    m_rawBody.clear();
    m_stoppedOnRepeat = false;

    m_reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, &LlmOcr::onFinished);
    if (!m_isAnthropic) {
        connect(m_reply, &QNetworkReply::readyRead, this, &LlmOcr::onStreamData);
    }
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

void LlmOcr::onStreamData()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr || reply != m_reply || m_stoppedOnRepeat) {
        return;
    }
    // readyRead can still arrive while an aborted reply is tearing down, and
    // reading a closed device just warns.
    if (!reply->isOpen()) {
        return;
    }

    const QByteArray incoming = reply->readAll();
    // readyRead drains the reply, so keep the bytes: a server that ignores
    // `stream` and answers with a plain JSON body must still be readable in
    // onFinished, and so must an error body.
    m_rawBody.append(incoming);
    m_streamBuffer.append(incoming);

    // Server-sent events: "data: {json}" per line, terminated by "data: [DONE]".
    int lineEnd = 0;
    while ((lineEnd = m_streamBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_streamBuffer.left(lineEnd).trimmed();
        m_streamBuffer.remove(0, lineEnd + 1);
        if (!line.startsWith("data:")) {
            continue;
        }
        const QByteArray payload = line.mid(5).trimmed();
        if (payload.isEmpty() || payload == "[DONE]") {
            continue;
        }
        const QJsonObject chunk = QJsonDocument::fromJson(payload).object();
        const QJsonArray choices = chunk.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            continue;
        }
        const QJsonObject delta = choices.first().toObject().value(QStringLiteral("delta")).toObject();
        m_streamText += delta.value(QStringLiteral("content")).toString();
    }

    // The instant the transcription starts saying itself again, everything
    // after that point is the loop - so keep what came before it and stop the
    // model rather than paying for the rest.
    const int offset = repeatOffset(m_streamText);
    if (offset > 0) {
        m_stoppedOnRepeat = true;
        const QString transcription = m_streamText.left(offset).trimmed();
        m_streamText = transcription;
        reply->abort();
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
        if (m_stoppedOnRepeat) {
            // Our own abort, once the whole transcription was in hand.
            emit recognized(m_streamText);
        } else if (m_userCanceled) {
            emit canceled();
        } else {
            emit failed(tr("LLM OCR request timed out"));
        }
        return;
    }

    // A stream that ended on its own: the text arrived in deltas, not in the
    // body. Still run it through the collapse, since generation may have
    // finished a repeat before the detector had enough to see it.
    if (!m_isAnthropic && !m_streamText.isEmpty()) {
        emit recognized(collapseRepeatedTranscription(m_streamText));
        return;
    }

    // Whatever readyRead already consumed plus anything still buffered. An
    // aborted reply is closed and reading it only warns.
    const QByteArray payload = m_rawBody + (reply->isOpen() ? reply->readAll() : QByteArray());
    const OpenAiEndpoint::Completion completion = OpenAiEndpoint::parseCompletion(payload, m_isAnthropic);

    if (reply->error() != QNetworkReply::NoError) {
        // The server's own message names the actual problem; Qt's transport
        // string only ever says the request failed.
        emit failed(tr("LLM OCR error: %1").arg(completion.errorMessage.isEmpty() ? reply->errorString() : completion.errorMessage));
        return;
    }

    if (!completion.hasText()) {
        emit failed(emptyResponseReason(completion));
        return;
    }

    emit recognized(collapseRepeatedTranscription(completion.content));
}
