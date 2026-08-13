/*
 * SPDX-FileCopyrightText: 2026 Oleksandr Mikriukov <ur3ley@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "localaitranslationprovider.h"

#include "provideroptions.h"
#include "settings/appsettings.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QUrl>

LocalAiTranslationProvider::LocalAiTranslationProvider(QObject *parent)
    : ATranslationProvider(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_detectReply(nullptr)
    , m_url(AppSettings::defaultLocalProviderUrl(QStringLiteral("ollama")))
    , m_model(AppSettings::defaultLocalProviderModel(QStringLiteral("ollama")))
    , m_prompt(AppSettings::defaultLocalAiPrompt())
    , m_detectViaLlm(false)
    , m_detectUrl(AppSettings::defaultLocalProviderUrl(QStringLiteral("ollama")))
    , m_detectModel()
    , m_sourceWasAuto(false)
    , m_detectThenTranslate(false)
    , m_userAborted(false)
{
    emit stateChanged(state);
}

LocalAiTranslationProvider::~LocalAiTranslationProvider() = default;

QString LocalAiTranslationProvider::getProviderType() const
{
    return QStringLiteral("LocalAiTranslationProvider");
}

QString LocalAiTranslationProvider::completionsUrl(const QString &baseUrl, bool isAnthropic)
{
    QString base = baseUrl;
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return base + (isAnthropic ? QStringLiteral("/v1/messages") : QStringLiteral("/v1/chat/completions"));
}

void LocalAiTranslationProvider::setAuthHeaders(QNetworkRequest &request, bool isAnthropic, const QString &apiKey)
{
    if (apiKey.isEmpty()) {
        return;
    }
    if (isAnthropic) {
        request.setRawHeader("x-api-key", apiKey.toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");
    } else {
        request.setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());
    }
}

// ── Supported languages ───────────────────────────────────────

QVector<Language> LocalAiTranslationProvider::supportedSourceLanguages()
{
    const QList<Language> all = Language::allLanguages();
    return QVector<Language>(all.begin(), all.end());
}

QVector<Language> LocalAiTranslationProvider::supportedDestinationLanguages()
{
    return supportedSourceLanguages();
}

bool LocalAiTranslationProvider::supportsAutodetection() const
{
    return true;
}

// ── Supported languages ───────────────────────────────────────

Language LocalAiTranslationProvider::detectLanguage(const QString &text)
{
    Q_UNUSED(text);
    const Language lang = AppSettings().primaryLanguage();
    sourceLanguage = lang;
    emit languageDetected(lang, false);
    return lang;
}

void LocalAiTranslationProvider::setSourceImage(const QByteArray &imageData)
{
    m_imageData = imageData;
}

void LocalAiTranslationProvider::clearSourceImage()
{
    m_imageData.clear();
}

bool LocalAiTranslationProvider::hasSourceImage() const
{
    return !m_imageData.isEmpty();
}

// ── Prompt & formatting ───────────────────────────────────────

QString LocalAiTranslationProvider::languageDisplayName(const QString &code)
{
    return Language(code).name();
}

QString LocalAiTranslationProvider::buildPrompt(const QString &srcCode, const QString &dstCode, const QString &text) const
{
    const QString srcName = languageDisplayName(srcCode);
    const QString dstName = languageDisplayName(dstCode);

    QString p = m_prompt.isEmpty() ? AppSettings::defaultLocalAiPrompt() : m_prompt;
    p.replace(QStringLiteral("{source_lang}"), srcName);
    p.replace(QStringLiteral("{source_code}"), srcCode);
    p.replace(QStringLiteral("{target_lang}"), dstName);
    p.replace(QStringLiteral("{target_code}"), dstCode);

    if (p.contains(QStringLiteral("{text}"))) {
        p.replace(QStringLiteral("{text}"), text);
    } else {
        p += QStringLiteral("\n\n\n") + text;
    }
    return p;
}

QString LocalAiTranslationProvider::formatResult(const QString &text)
{
    QString t = text.trimmed();
    return t.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>"));
}

// ── Translation ───────────────────────────────────────────────

void LocalAiTranslationProvider::translate(const QString &inputText, const Language &translationLang, const Language &sourceLang)
{
    if (state == State::Processing) {
        return;
    }

    sourceLanguage = sourceLang;
    translationLanguage = translationLang;

    state = State::Processing;
    error = TranslationError::NoError;
    result.clear();
    emit stateChanged(state);

    const QString dstCode = translationLang.toCode();
    if (dstCode.isEmpty() || translationLang == Language::autoLanguage()) {
        state = State::Finished;
        error = TranslationError::UnsupportedDstLanguage;
        errorString = QStringLiteral("Destination language is not supported");
        emit stateChanged(state);
        return;
    }

    m_sourceWasAuto = (sourceLang == Language::autoLanguage());

    // Model-based detection (opt-in): detect first, then translate.
    if (m_sourceWasAuto && m_detectViaLlm && !m_detectModel.isEmpty()) {
        m_detectThenTranslate = true;
        m_pendingDstCode = dstCode;
        sendDetection(inputText);
        return;
    }

    Language srcLang = sourceLang;
    if (m_sourceWasAuto) {
        srcLang = AppSettings().primaryLanguage();
        sourceLanguage = srcLang;
    }
    const QString srcCode = srcLang.toCode();

    if (srcCode == dstCode && !hasSourceImage()) {
        result = formatResult(inputText);
        error = TranslationError::NoError;
        state = State::Processed;
        if (m_sourceWasAuto) {
            emit languageDetected(srcLang, true);
        }
        emit stateChanged(state);
        return;
    }

    sendTranslation(srcCode, dstCode, inputText);
}

void LocalAiTranslationProvider::sendTranslation(const QString &srcCode, const QString &dstCode, const QString &text)
{
    if (m_reply != nullptr) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    const bool isVision = !m_imageData.isEmpty();

    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("user"));

    if (isVision) {
        const QString srcName = languageDisplayName(srcCode);
        const QString dstName = languageDisplayName(dstCode);
        QString vp = m_visionPrompt.isEmpty() ? AppSettings::defaultVisionPrompt() : m_visionPrompt;
        vp.replace(QStringLiteral("{source_lang}"), srcName);
        vp.replace(QStringLiteral("{source_code}"), srcCode);
        vp.replace(QStringLiteral("{target_lang}"), dstName);
        vp.replace(QStringLiteral("{target_code}"), dstCode);

        QJsonArray content;
        QJsonObject textPart;
        textPart.insert(QStringLiteral("type"), QStringLiteral("text"));
        textPart.insert(QStringLiteral("text"), vp);
        content.append(textPart);

        QJsonObject imagePart;
        const QString base64Data = QString::fromLatin1(m_imageData.toBase64());
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
        content.append(imagePart);

        message.insert(QStringLiteral("content"), content);
    } else {
        const QString prompt = buildPrompt(srcCode, dstCode, text);
        message.insert(QStringLiteral("content"), prompt);
    }

    QJsonArray messages;
    messages.append(message);

    const QString model = isVision ? m_visionModel : m_model;

    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("temperature"), 0.0);
    if (m_isAnthropic) {
        // Anthropic's Messages API requires max_tokens; the other shapes
        // default it server-side.
        body.insert(QStringLiteral("max_tokens"), 4096);
    } else {
        body.insert(QStringLiteral("stream"), false);
        if ((isVision ? m_visionDisableThinking : m_disableThinking)) {
            body.insert(QStringLiteral("reasoning_effort"), QStringLiteral("none"));
        }
    }

    QNetworkRequest request(QUrl(completionsUrl(m_url, m_isAnthropic)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    setAuthHeaders(request, m_isAnthropic, m_apiKey);
    m_network->setTransferTimeout((isVision ? m_visionTimeout : m_timeout) * 1000);

    m_reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, &LocalAiTranslationProvider::onReplyFinished);
}

static QString detectPrompt()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/detect-prompt.txt");
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QTextStream(&file).readAll().trimmed();
        if (!content.isEmpty()) {
            return content;
        }
    }
    return QStringLiteral(
        "What language is the following text? Reply in exactly this format — "
        "Language name, colon, space, then ISO 639-1 code. "
        "Examples: English: EN, Russian: RU, German: DE, French: FR, "
        "Spanish: ES, Japanese: JA, Chinese: ZH, Korean: KO. "
        "Only the name and code, nothing else.");
}

void LocalAiTranslationProvider::sendDetection(const QString &text)
{
    if (m_detectReply != nullptr) {
        m_detectReply->disconnect(this);
        m_detectReply->abort();
        m_detectReply->deleteLater();
        m_detectReply = nullptr;
    }

    m_pendingText = text;

    const QString prompt = detectPrompt() + QStringLiteral("\n\n\n") + text;

    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("user"));
    message.insert(QStringLiteral("content"), prompt);
    QJsonArray messages;
    messages.append(message);

    QJsonObject body;
    body.insert(QStringLiteral("model"), m_detectModel);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("temperature"), 0.0);
    if (m_detectIsAnthropic) {
        body.insert(QStringLiteral("max_tokens"), 64);
    } else {
        body.insert(QStringLiteral("stream"), false);
        if (m_disableThinking) {
            body.insert(QStringLiteral("reasoning_effort"), QStringLiteral("none"));
        }
    }

    QNetworkRequest request(QUrl(completionsUrl(m_detectUrl, m_detectIsAnthropic)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    setAuthHeaders(request, m_detectIsAnthropic, m_detectApiKey);
    m_network->setTransferTimeout(m_timeout * 1000);

    m_detectReply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_detectReply, &QNetworkReply::finished, this, &LocalAiTranslationProvider::onDetectFinished);
}

static QString extractContent(const QByteArray &data, bool isAnthropic)
{
    const QJsonObject obj = QJsonDocument::fromJson(data).object();
    if (isAnthropic) {
        const QJsonArray blocks = obj.value(QStringLiteral("content")).toArray();
        for (const QJsonValue &block : blocks) {
            const QJsonObject blockObj = block.toObject();
            if (blockObj.value(QStringLiteral("type")).toString() == QLatin1String("text")) {
                return blockObj.value(QStringLiteral("text")).toString();
            }
        }
        return QString();
    }
    const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        return choices.first().toObject().value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
    }
    // Fallback for Ollama native shape, just in case.
    return obj.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
}

void LocalAiTranslationProvider::onReplyFinished()
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
        if (!m_userAborted) {
            error = TranslationError::Custom;
            errorString = QStringLiteral("LocalAI request timed out");
            state = State::Finished;
            emit stateChanged(state);
        }
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        error = TranslationError::Custom;
        errorString = QStringLiteral("LocalAI error: ") + reply->errorString();
        state = State::Finished;
        emit stateChanged(state);
        return;
    }

    const QString translated = extractContent(reply->readAll(), m_isAnthropic);
    if (translated.isEmpty()) {
        error = TranslationError::Custom;
        errorString = QStringLiteral("LocalAI returned an empty response");
        state = State::Finished;
        emit stateChanged(state);
        return;
    }

    result = formatResult(translated);
    error = TranslationError::NoError;
    state = State::Processed;
    if (m_sourceWasAuto) {
        emit languageDetected(sourceLanguage, true);
    }
    emit stateChanged(state);
}

void LocalAiTranslationProvider::onDetectFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr) {
        return;
    }

    if (reply != m_detectReply) {
        reply->deleteLater();
        return;
    }
    m_detectReply = nullptr;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        if (!m_userAborted) {
            if (m_detectThenTranslate) {
                m_detectThenTranslate = false;
                error = TranslationError::Custom;
                errorString = QStringLiteral("Language detection timed out");
                state = State::Finished;
            } else {
                error = TranslationError::NoError;
                state = State::Ready;
            }
            emit stateChanged(state);
        }
        return;
    }

    QString code;
    if (reply->error() == QNetworkReply::NoError) {
        const QString resp = extractContent(reply->readAll(), m_detectIsAnthropic).toLower();
        static const QRegularExpression re(QStringLiteral("\\b[a-z]{2,3}([-][A-Za-z0-9]+)*\\b"));
        QRegularExpressionMatchIterator it = re.globalMatch(resp);
        if (it.hasNext()) {
            code = it.next().captured(0);
        }
    }

    Language detected = code.isEmpty() ? AppSettings().primaryLanguage() : Language(code);
    if (detected == Language::autoLanguage()) {
        detected = AppSettings().primaryLanguage();
    }
    sourceLanguage = detected;

    if (m_detectThenTranslate) {
        m_detectThenTranslate = false;
        const QString srcCode = detected.toCode();
        emit languageDetected(detected, true);

        if (srcCode == m_pendingDstCode && !hasSourceImage()) {
            result = formatResult(m_pendingText);
            error = TranslationError::NoError;
            state = State::Processed;
            emit stateChanged(state);
            return;
        }
        sendTranslation(srcCode, m_pendingDstCode, m_pendingText);
        return;
    }

    error = TranslationError::NoError;
    state = State::Ready;
    emit languageDetected(detected, false);
    emit stateChanged(state);
}

void LocalAiTranslationProvider::abort()
{
    m_userAborted = true;
    if (m_reply != nullptr) {
        m_reply->abort();
    }
    if (m_detectReply != nullptr) {
        m_detectReply->abort();
    }
    m_detectThenTranslate = false;
    state = State::Finished;
    error = TranslationError::Aborted;
    errorString = QStringLiteral("Translation aborted by user");
    result.clear();
    m_userAborted = false;
    emit stateChanged(state);
}

// ── Options ───────────────────────────────────────────────────

void LocalAiTranslationProvider::applyOptions(const ProviderOptions &options)
{
    if (options.hasOption("url")) {
        m_url = options.getOption("url").toString();
    }
    if (options.hasOption("model")) {
        m_model = options.getOption("model").toString();
    }
    if (options.hasOption("prompt")) {
        m_prompt = options.getOption("prompt").toString();
    }
    if (options.hasOption("disable_thinking")) {
        m_disableThinking = options.getOption("disable_thinking").toBool();
    }
    if (options.hasOption("timeout")) {
        m_timeout = options.getOption("timeout").toInt();
    }
    if (options.hasOption("api_key")) {
        m_apiKey = options.getOption("api_key").toString();
    }
    if (options.hasOption("is_anthropic")) {
        m_isAnthropic = options.getOption("is_anthropic").toBool();
    }
    if (options.hasOption("detect_via_llm")) {
        m_detectViaLlm = options.getOption("detect_via_llm").toBool();
    }
    if (options.hasOption("detect_url")) {
        m_detectUrl = options.getOption("detect_url").toString();
    }
    if (options.hasOption("detect_model")) {
        m_detectModel = options.getOption("detect_model").toString();
    }
    if (options.hasOption("detect_api_key")) {
        m_detectApiKey = options.getOption("detect_api_key").toString();
    }
    if (options.hasOption("detect_is_anthropic")) {
        m_detectIsAnthropic = options.getOption("detect_is_anthropic").toBool();
    }
    if (options.hasOption("vision_enabled")) {
        m_visionEnabled = options.getOption("vision_enabled").toBool();
    }
    if (options.hasOption("vision_model")) {
        m_visionModel = options.getOption("vision_model").toString();
    }
    if (options.hasOption("vision_prompt")) {
        m_visionPrompt = options.getOption("vision_prompt").toString();
    }
    if (options.hasOption("vision_disable_thinking")) {
        m_visionDisableThinking = options.getOption("vision_disable_thinking").toBool();
    }
    if (options.hasOption("vision_timeout")) {
        m_visionTimeout = options.getOption("vision_timeout").toInt();
    }
}

std::unique_ptr<ProviderOptions> LocalAiTranslationProvider::getDefaultOptions() const
{
    auto options = std::make_unique<ProviderOptions>();
    options->setOption("url", AppSettings::defaultLocalProviderUrl(QStringLiteral("ollama")));
    options->setOption("model", AppSettings::defaultLocalProviderModel(QStringLiteral("ollama")));
    options->setOption("prompt", AppSettings::defaultLocalAiPrompt());
    options->setOption("disable_thinking", false);
    options->setOption("timeout", AppSettings::defaultLocalAiTimeout());
    options->setOption("api_key", QString());
    options->setOption("is_anthropic", false);
    options->setOption("detect_via_llm", false);
    options->setOption("detect_url", AppSettings::defaultLocalProviderUrl(QStringLiteral("ollama")));
    options->setOption("detect_model", QString());
    options->setOption("detect_api_key", QString());
    options->setOption("detect_is_anthropic", false);
    options->setOption("vision_enabled", false);
    options->setOption("vision_model", QString());
    options->setOption("vision_prompt", AppSettings::defaultVisionPrompt());
    options->setOption("vision_disable_thinking", false);
    options->setOption("vision_timeout", AppSettings::defaultLocalAiTimeout());
    return options;
}

QStringList LocalAiTranslationProvider::getAvailableOptions() const
{
    return {QStringLiteral("url"), QStringLiteral("model"), QStringLiteral("prompt"),
            QStringLiteral("disable_thinking"), QStringLiteral("timeout"),
            QStringLiteral("api_key"), QStringLiteral("is_anthropic"),
            QStringLiteral("detect_via_llm"), QStringLiteral("detect_url"), QStringLiteral("detect_model"),
            QStringLiteral("detect_api_key"), QStringLiteral("detect_is_anthropic"),
            QStringLiteral("vision_enabled"), QStringLiteral("vision_model"), QStringLiteral("vision_prompt"),
            QStringLiteral("vision_disable_thinking"), QStringLiteral("vision_timeout")};
}

ProviderUIRequirements LocalAiTranslationProvider::getUIRequirements() const
{
    ProviderUIRequirements requirements;
    requirements.requiredUIElements = {QStringLiteral("engineComboBox")};
    requirements.supportedSignals = {QStringLiteral("languageDetected")};
    requirements.supportedCapabilities = {QStringLiteral("languageDetection"), QStringLiteral("providerSelection")};
    return requirements;
}

void LocalAiTranslationProvider::saveOptionToSettings(const QString &optionKey, const QVariant &value)
{
    Q_UNUSED(optionKey);
    Q_UNUSED(value);
    // Settings for LocalAI are managed via the settings dialog and the
    // provider selector; nothing to persist here directly.
}
