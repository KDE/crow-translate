/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "translationsession.h"

#include "modulestatus.h"
#include "provideroptionsmanager.h"
#include "core/usernotifier.h"
#include "ocr/aocrprovider.h"
#include "ocr/llmocr.h"
#include "ocr/tesseractocr.h"
#include "settings/appsettings.h"
#include "translator/languageresolution.h"
#include "translator/translationlogic.h"

#include <QFile>
#include <QLocale>
#include <QSet>
#include <QtEndian>

TranslationSession::TranslationSession(QObject *parent)
    : QObject(parent)
    , m_options(new ProviderOptionsManager(this))
    , m_languages(new LanguageResolution(this))
    , m_status(new ModuleStatus(this))
    , m_tesseractOcr(new TesseractOcr(this))
    , m_llmOcr(new LlmOcr(this))
{
    // Both engines, not just the active one: the active one switches per
    // settings read, and a status model that only followed the current choice
    // would stop reporting the moment it changed.
    m_status->bindOcr(m_tesseractOcr, m_llmOcr);
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
    if (AppSettings().ocrEngine() == AppSettings::OcrEngine::Llm) {
        return m_llmOcr;
    }
    return m_tesseractOcr;
}

TesseractOcr *TranslationSession::tesseractOcr() const
{
    return m_tesseractOcr;
}

LlmOcr *TranslationSession::llmOcr() const
{
    return m_llmOcr;
}

ATranslationProvider::ProviderBackend TranslationSession::translationBackend() const
{
    return m_translationBackend.value_or(ATranslationProvider::ProviderBackend::Copy);
}

ATTSProvider::ProviderBackend TranslationSession::ttsBackend() const
{
    return m_ttsBackend.value_or(ATTSProvider::ProviderBackend::None);
}

void TranslationSession::setTranslationBackend(ATranslationProvider::ProviderBackend backend)
{
    if (m_translationBackend == backend) {
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

    m_translationBackend = backend;
    m_translator = ATranslationProvider::createTranslationProvider(this, backend);
    m_status->bindTranslator(m_translator);
    connectTranslator();
    applyTranslationOptions();

    Q_EMIT translatorChanged();
}

void TranslationSession::setTtsBackend(ATTSProvider::ProviderBackend backend)
{
    if (m_ttsBackend == backend) {
        return;
    }

    if (m_tts != nullptr) {
        m_tts->deleteLater();
    }

    m_ttsBackend = backend;
    m_tts = ATTSProvider::createTTSProvider(this, backend);
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
    });
    connect(m_translator, &ATranslationProvider::languageDetected, this, [this](const Language &detected, bool isTranslationContext) {
        m_languages->setDetectedSource(detected);
        Q_EMIT languageDetected(detected, isTranslationContext);
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
    ProviderOptionsManager::validateTTSBackendAvailability();

    const AppSettings settings;
    setTtsBackend(settings.ttsProviderBackend());
    setTranslationBackend(settings.translationProviderBackend());
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

void TranslationSession::requestTranslation(const QString &text, const Language &destination, const Language &source)
{
    if (m_translator == nullptr) {
        return;
    }

    Language actualDestination = destination;
    if (destination == Language::autoLanguage()) {
        // A known source answers the preference rule outright. An unknown one
        // cannot, so this is a placeholder: detection reports the real source
        // shortly afterwards, and the retranslate path corrects the
        // destination if the rule then lands somewhere else.
        actualDestination = preferredDestination(source == Language::autoLanguage() ? Language(QLocale::system()) : source);
    }

    Q_EMIT translationStarted(text, actualDestination, source);
    m_translator->translate(text, actualDestination, source);
}

void TranslationSession::requestTranslationOfSelection(const QString &text)
{
    requestTranslation(text, m_languages->selectedDestination(), m_languages->selectedSource());
}

void TranslationSession::abortTranslation()
{
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

void TranslationSession::initTesseractFromSettings()
{
    const AppSettings settings;
    const QByteArray languages = settings.ocrLanguagesString();
    const QByteArray path = settings.ocrLanguagesPath();
    if (!m_tesseractOcr->init(languages, path, settings.tesseractParameters())) {
        if (languages != AppSettings::defaultOcrLanguagesString() || path != AppSettings::defaultOcrLanguagesPath()) {
            Q_EMIT ocrLanguagesUnavailable(QString::fromUtf8(languages));
        }
    }
    m_tesseractOcr->setConvertLineBreaks(settings.isConvertLineBreaks());
}

void TranslationSession::configureLlmOcr()
{
    const AppSettings settings;
    const QString providerId = settings.ocrLlmProvider();
    m_llmOcr->setEndpoint(settings.localProviderUrl(providerId), AppSettings::localProviderIsAnthropic(providerId), settings.localProviderApiKey(providerId));
    m_llmOcr->setModel(settings.ocrLlmModel(providerId));
    m_llmOcr->setTimeout(settings.ocrLlmTimeout(providerId));
    m_llmOcr->setPrompt(settings.ocrLlmPrompt(settings.ocrLlmModel(providerId)));
    m_llmOcr->setDisableThinking(settings.localAiDisableThinking(providerId));
}

bool TranslationSession::prepareOcr()
{
    configureLlmOcr();
    if (ocr()->isConfigured()) {
        return true;
    }

    // Deliberately still TesseractOcr::tr(): the strings are unchanged and so
    // is their translation context, so no catalogue entry has to be re-matched
    // for having moved house.
    UserNotifier::Notification notification;
    notification.severity = UserNotifier::Severity::Critical;
    notification.title = TesseractOcr::tr("OCR is not configured");
    notification.text = TesseractOcr::tr("Set up the OCR engine in the application settings");
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
