/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "translationsession.h"

#include "modulestatus.h"
#include "provideroptionsmanager.h"
#include "core/pluginmanager.h"
#include "core/usernotifier.h"
#include "ocr/aocrprovider.h"
#include "settings/appsettings.h"
#include "translator/languageresolution.h"
#include "translator/translationlogic.h"

#include <QCoreApplication>
#include <QFile>
#include <QLocale>
#include <QSet>
#include <QtEndian>

TranslationSession::TranslationSession(QObject *parent)
    : QObject(parent)
    , m_options(new ProviderOptionsManager(this))
    , m_languages(new LanguageResolution(this))
    , m_status(new ModuleStatus(this))
{
    // Every installed engine, not just the active one: the active one
    // switches per settings read, and a status model that only followed the
    // current choice would stop reporting the moment it changed.
    const QList<PluginInfo> engines = PluginManager::instance()->available(PluginFamily::Ocr);
    for (const PluginInfo &engine : engines) {
        AOcrProvider *provider = PluginManager::instance()->createOcrProvider(this, engine.id);
        if (provider == nullptr)
            continue;
        m_ocrEngines.insert(engine.id, provider);
        connect(provider, &AOcrProvider::configurationFailed, this, [this](const QString &detail) {
            if (!m_ocrConfigurationIsDefault)
                Q_EMIT ocrLanguagesUnavailable(detail);
        });
    }
    m_status->bindOcr(ocrEngines());
}

ATranslationProvider *TranslationSession::translator() const
{
    return m_translator;
}

ATTSProvider *TranslationSession::tts() const
{
    return m_tts;
}

ProviderOptionsManager *TranslationSession::options() const
{
    return m_options;
}

LanguageResolution *TranslationSession::languages() const
{
    return m_languages;
}

ModuleStatus *TranslationSession::moduleStatus() const
{
    return m_status;
}

AOcrProvider *TranslationSession::ocr() const
{
    return m_ocrEngines.value(ocrEngineId(), nullptr);
}

QString TranslationSession::ocrEngineId() const
{
    return PluginManager::selectedId(PluginFamily::Ocr);
}

QList<AOcrProvider *> TranslationSession::ocrEngines() const
{
    return m_ocrEngines.values();
}

QString TranslationSession::translationBackend() const
{
    return m_translationBackendId.isEmpty() ? PluginManager::fallbackId(PluginFamily::Translation) : m_translationBackendId;
}

QString TranslationSession::ttsBackend() const
{
    return m_ttsBackendId.isEmpty() ? PluginManager::fallbackId(PluginFamily::Tts) : m_ttsBackendId;
}

void TranslationSession::setTranslationBackend(const QString &id)
{
    if (m_translationBackendId == id) {
        return;
    }

    // Nothing is disconnected by hand here. Every connection made below has
    // the provider as one end, so destroying it severs all of them - which is
    // the point of the provider living behind this class rather than beside
    // the window's own signals, where each swap had to unpick and redo the
    // same four connections and could silently miss one.
    if (m_translator != nullptr) {
        m_translator->deleteLater();
    }
    // A question put to the old backend is not going to be answered by it.
    const bool unanswered = m_languageQuestion.has_value();

    m_translationBackendId = id;
    m_translator = PluginManager::instance()->createTranslationProvider(this, id);
    m_status->bindTranslator(m_translator);
    connectTranslator();
    applyTranslationOptions();

    Q_EMIT translatorChanged();

    if (unanswered) {
        answerLanguageQuestion(m_languages->effectiveSource());
    }
}

void TranslationSession::setTtsBackend(const QString &id)
{
    if (m_ttsBackendId == id) {
        return;
    }

    if (m_tts != nullptr) {
        m_tts->deleteLater();
    }

    m_ttsBackendId = id;
    m_tts = PluginManager::instance()->createTtsProvider(this, id);
    m_status->bindTtsProvider(m_tts);
    connectTts();
    applyTtsOptions();

    Q_EMIT ttsProviderChanged();
}

void TranslationSession::connectTranslator()
{
    connect(m_translator, &ATranslationProvider::stateChanged, this, [this](ATranslationProvider::State newState) {
        // What the provider is actually translating between. It records this
        // when the request goes out, so following its state keeps the answer
        // current rather than re-reading it from whoever happens to ask.
        m_languages->setTranslated(m_translator->sourceLanguage, m_translator->translationLanguage);
        Q_EMIT translationStateChanged(newState);

        if (m_languageQuestion.has_value()) {
            if (!m_languageQuestion->asked) {
                askLanguageQuestion();
            } else {
                // Asked, and the backend has moved on without saying: it
                // timed out, was aborted, or was given something else to do.
                answerLanguageQuestion(m_languages->effectiveSource());
            }
        }
    });
    connect(m_translator, &ATranslationProvider::partialTranslation, this, &TranslationSession::translationPartial);
    connect(m_translator, &ATranslationProvider::languageDetected, this, [this](const Language &detected, bool isTranslationContext) {
        m_languages->setDetectedSource(detected);
        if (isTranslationContext) {
            followDetection(detected);
        }
        Q_EMIT languageDetected(detected, isTranslationContext);

        if (!isTranslationContext && m_languageQuestion.has_value() && m_languageQuestion->asked) {
            answerLanguageQuestion(detected);
        }
    });
    connect(m_translator, &ATranslationProvider::engineChanged, this, &TranslationSession::engineChanged);
}

void TranslationSession::connectTts()
{
    connect(m_tts, &ATTSProvider::stateChanged, this, &TranslationSession::ttsStateChanged);
    connect(m_tts, &ATTSProvider::errorOccurred, this, &TranslationSession::ttsErrorOccurred);
}

void TranslationSession::loadBackendsFromSettings()
{
    // Validation can rewrite the stored choice - a backend that was built
    // into the last version and is not in this one, say - so it runs before
    // the value is read, not after.

    const AppSettings settings;
    setTtsBackend(settings.ttsProviderId());
    setTranslationBackend(settings.translationProviderId());
}

void TranslationSession::applyTranslationOptions()
{
    if (m_translator == nullptr) {
        return;
    }

    m_options->applySettingsToTranslationProvider(m_translator);
}

void TranslationSession::applyTtsOptions()
{
    if (m_tts == nullptr) {
        return;
    }

    m_options->applySettingsToTTSProvider(m_tts);
}

void TranslationSession::setSelectedLanguages(const Language &source, const Language &destination)
{
    const AppSettings settings;
    m_languages->setPreference(settings.primaryLanguage(), settings.secondaryLanguage(), Language(QLocale::system()));
    m_languages->setSelected(source, destination);
}

Language TranslationSession::preferredDestination(const Language &source) const
{
    const AppSettings settings;
    return TranslationLogic::preferredDestination(source,
                                                  settings.primaryLanguage(),
                                                  settings.secondaryLanguage(),
                                                  Language(QLocale::system()));
}

TranslationLogic::ForcedAuto TranslationSession::forcedAutoFor(TranslationLogic::ForcedFlow flow) const
{
    const AppSettings settings;
    const bool engineDetects = m_translator != nullptr && m_translator->supportsAutodetection();
    return TranslationLogic::forcedAuto(flow, settings.isForceSourceAutodetect(), settings.isForceTranslationAutodetect(), engineDetects);
}

void TranslationSession::requestTranslation(const QString &text, const Language &destination, const Language &source)
{
    if (m_translator == nullptr) {
        return;
    }

    m_automaticDestination = {};
    Language actualDestination = destination;
    if (destination == Language::autoLanguage()) {
        // A known source answers the preference rule outright. An unknown one
        // cannot, so this is a guess, remembered as one: the provider reports
        // the real source shortly, and followDetection() puts it right.
        const bool sourceUnknown = source == Language::autoLanguage();
        actualDestination = preferredDestination(sourceUnknown ? Language(QLocale::system()) : source);
        if (sourceUnknown) {
            m_automaticDestination = {true, text, actualDestination};
        }
    }

    Q_EMIT translationStarted(text, actualDestination, source);
    m_translator->translate(text, actualDestination, source);
}

// The one place where "the destination is automatic" meets "now we know the
// source". It was three: the window asked again when a detection arrived with
// a finished translation, the command line did the same on its own once the
// translation was through, and a backend that detects before it translates
// was nobody's business - it went on to the guess, and that was the answer
// the user got.
void TranslationSession::followDetection(const Language &detected)
{
    if (!m_automaticDestination.pending || m_translator == nullptr) {
        return;
    }
    // The source is known from here on, so this happens once per request.
    m_automaticDestination.pending = false;

    const Language preferred = preferredDestination(detected);
    if (preferred == m_automaticDestination.guess) {
        return;
    }

    if (m_translator->redirect(preferred)) {
        // Not asked for yet: it goes to the right language in the first place.
        m_languages->setTranslated(m_translator->sourceLanguage, m_translator->translationLanguage);
        return;
    }
    if (m_translator->getState() == ATranslationProvider::State::Processed) {
        // The detection came with the translation, into the guess. Once more.
        // The provider has not announced that result yet and now will not:
        // the next thing a frontend hears is this request's.
        qDebug() << "Retranslating from" << detected.name() << "to preferred destination:" << preferred.name();
        Q_EMIT translationStarted(m_automaticDestination.text, preferred, detected);
        m_translator->translate(m_automaticDestination.text, preferred, detected);
    }
}

void TranslationSession::requestTranslationOfSelection(const QString &text)
{
    requestTranslation(text, m_languages->selectedDestination(), m_languages->selectedSource());
}

void TranslationSession::findSourceLanguage(const QString &text)
{
    m_languageQuestion = LanguageQuestion{text, false};

    const bool automatic = m_languages->selectedSource() == Language::autoLanguage();
    if (!automatic || text.isEmpty() || m_translator == nullptr || !m_translator->supportsAutodetection()) {
        answerLanguageQuestion(m_languages->effectiveSource());
        return;
    }
    askLanguageQuestion();
}

// Puts the question to the backend, if it is free; the state change that says
// it is comes back here otherwise.
void TranslationSession::askLanguageQuestion()
{
    if (m_translator->getState() != ATranslationProvider::State::Ready) {
        return;
    }
    m_languageQuestion->asked = true;
    // A copy: a backend that knows at once answers from inside this call, and
    // the question is gone by the time it returns.
    const QString text = m_languageQuestion->text;
    m_translator->detectLanguage(text);
}

void TranslationSession::answerLanguageQuestion(const Language &language)
{
    const QString text = m_languageQuestion->text;
    m_languageQuestion.reset();
    Q_EMIT sourceLanguageFound(text, language);
}

void TranslationSession::abortTranslation()
{
    m_automaticDestination = {};
    if (m_translator != nullptr) {
        m_translator->abort();
    }
}

void TranslationSession::acceptTranslation()
{
    if (m_translator != nullptr) {
        m_translator->finish();
    }
}

void TranslationSession::resetTranslator()
{
    m_automaticDestination = {};
    if (m_translator != nullptr) {
        m_translator->reset();
    }
}

namespace
{

// Qt refuses some PNGs outright over a chunk it does not know - screenshots
// from certain tools carry private chunks - so a failed load is retried with
// everything non-standard removed rather than reported as a broken file.
QByteArray stripNonStandardPngChunks(const QByteArray &data)
{
    constexpr int SIG_LEN = 8;
    if (data.size() < SIG_LEN + 12 || data.left(SIG_LEN) != QByteArray("\x89PNG\r\n\x1a\n", SIG_LEN)) {
        return data;
    }

    QByteArray out = data.left(SIG_LEN);

    static const QSet<QByteArray> standardChunks = {
        QByteArrayLiteral("IHDR"), QByteArrayLiteral("PLTE"),
        QByteArrayLiteral("IDAT"), QByteArrayLiteral("IEND"),
        QByteArrayLiteral("tRNS"), QByteArrayLiteral("cHRM"), QByteArrayLiteral("gAMA"),
        QByteArrayLiteral("iCCP"), QByteArrayLiteral("sBIT"), QByteArrayLiteral("sRGB"),
        QByteArrayLiteral("bKGD"), QByteArrayLiteral("hIST"), QByteArrayLiteral("tEXt"),
        QByteArrayLiteral("zTXt"), QByteArrayLiteral("iTXt"), QByteArrayLiteral("pHYs"),
        QByteArrayLiteral("sPLT"), QByteArrayLiteral("tIME"), QByteArrayLiteral("eXIf"),
        QByteArrayLiteral("oFFs"), QByteArrayLiteral("pCAL"), QByteArrayLiteral("sCAL"),
        QByteArrayLiteral("gIFg"), QByteArrayLiteral("gIFx")};

    int pos = SIG_LEN;
    while (pos + 12 <= data.size()) {
        const quint32 length = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + pos));
        const QByteArray type = data.mid(pos + 4, 4);
        const int chunkTotal = 12 + static_cast<int>(length);

        if (pos + chunkTotal > data.size()) {
            return data;
        }

        if (standardChunks.contains(type)) {
            out.append(data.mid(pos, chunkTotal));
        }

        pos += chunkTotal;
        if (type == QByteArrayLiteral("IEND")) {
            break;
        }
    }

    return out;
}

} // namespace

bool TranslationSession::loadImage(const QString &path, QImage &out)
{
    if (out.load(path)) {
        return true;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray raw = file.readAll();
    const QByteArray clean = stripNonStandardPngChunks(raw);
    if (clean != raw) {
        out.loadFromData(clean);
    }
    return !out.isNull();
}

void TranslationSession::applyOcrOptions()
{
    const AppSettings settings;

    const QByteArray languages = settings.ocrLanguagesString();
    const QByteArray languagesPath = settings.ocrLanguagesPath();
    m_ocrConfigurationIsDefault = languages == AppSettings::defaultOcrLanguagesString()
        && languagesPath == AppSettings::defaultOcrLanguagesPath();

    if (AOcrProvider *tesseract = m_ocrEngines.value(QStringLiteral("tesseract"), nullptr)) {
        ProviderOptions options;
        options.setOption(QStringLiteral("languages"), languages);
        options.setOption(QStringLiteral("languages_path"), languagesPath);
        options.setOption(QStringLiteral("parameters"), settings.tesseractParameters());
        options.setOption(QStringLiteral("convert_line_breaks"), settings.isConvertLineBreaks());
        tesseract->applyOptions(options);
    }

    if (AOcrProvider *llm = m_ocrEngines.value(QStringLiteral("llm"), nullptr)) {
        // Its whole configuration hangs off whichever endpoint is active, so
        // there is no flat key to read and the host resolves it first. Same
        // shape as the LocalAI translation backend, and for the same reason.
        const QString endpointId = settings.ocrLlmProvider();
        ProviderOptions options;
        // The endpoint from the recognition page. It used to be the
        // translation one: the address typed on the recognition page was
        // saved and never read.
        options.setOption(QStringLiteral("url"), settings.ocrLlmUrl(endpointId));
        options.setOption(QStringLiteral("is_anthropic"), AppSettings::localProviderIsAnthropic(endpointId));
        options.setOption(QStringLiteral("api_key"), settings.ocrLlmApiKey(endpointId));
        options.setOption(QStringLiteral("model"), settings.ocrLlmModel(endpointId));
        options.setOption(QStringLiteral("timeout"), settings.ocrLlmTimeout(endpointId));
        options.setOption(QStringLiteral("prompt"), settings.ocrLlmPrompt(settings.ocrLlmModel(endpointId)));
        options.setOption(QStringLiteral("disable_thinking"), settings.localAiDisableThinking(endpointId));
        llm->applyOptions(options);
    }
}

bool TranslationSession::prepareOcr()
{
    applyOcrOptions();
    AOcrProvider *engine = ocr();
    if (engine != nullptr && engine->isConfigured()) {
        return true;
    }

    if (engine == nullptr) {
        // Not the same thing as an engine that is installed and unconfigured,
        // and telling someone to set up a backend they do not have would send
        // them looking for a setting that is not there.
        UserNotifier::Notification missing;
        missing.severity = UserNotifier::Severity::Critical;
        missing.title = tr("No OCR engine available");
        missing.text = tr("No text recognition plugin is installed.");
        missing.requiresAcknowledgement = true;
        UserNotifier::notify(missing);
        return false;
    }

    // Deliberately still translated in TesseractOcr's context: the strings are
    // unchanged and so is their translation context, so no catalogue entry has
    // to be re-matched for having moved house. Named rather than called
    // through the class, which now lives in a module this cannot link.
    UserNotifier::Notification notification;
    notification.severity = UserNotifier::Severity::Critical;
    notification.title = QCoreApplication::translate("TesseractOcr", "OCR is not configured");
    notification.text = QCoreApplication::translate("TesseractOcr", "Set up the OCR engine in the application settings");
    notification.requiresAcknowledgement = true;
    UserNotifier::notify(notification);
    return false;
}

void TranslationSession::armOcrTranslation(AOcrProvider *engine)
{
    disarmOcrTranslation();
    m_ocrRecognizedConnection = connect(engine, &AOcrProvider::recognized, this, [this](const QString &text) {
        disarmOcrTranslation();
        Q_EMIT ocrTranslationChaining();

        const Language source = m_languages->selectedSource();
        const Language destination = m_languages->selectedDestination();

        if (m_translator != nullptr && m_translator->getState() == ATranslationProvider::State::Ready) {
            requestTranslation(text, destination, source);
            return;
        }

        // Wait for the translator to be ready. Held as a member for the same
        // reason as the recognition connection: it has to outlive this call.
        disconnect(m_translatorReadyConnection);
        if (m_translator == nullptr) {
            return;
        }
        m_translatorReadyConnection =
            connect(m_translator, &ATranslationProvider::stateChanged, this, [this, text, source, destination](ATranslationProvider::State state) {
                if (state == ATranslationProvider::State::Ready) {
                    disconnect(m_translatorReadyConnection);
                    requestTranslation(text, destination, source);
                }
            });
    });
}

void TranslationSession::disarmOcrTranslation()
{
    disconnect(m_ocrRecognizedConnection);
    disconnect(m_translatorReadyConnection);
}

bool TranslationSession::isOcrTranslationArmed() const
{
    return static_cast<bool>(m_ocrRecognizedConnection);
}
