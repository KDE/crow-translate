/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qtttsprovider.h"

#include "language.h"
#include "provideroptions.h"
#include "tts/attsprovider.h"
#include "tts/voice.h"

#include <QDebug>
#include <QMessageBox>
#include <QSet>

// Helper functions to convert between Voice and QVoice
static Voice qvoiceToVoice(const QVoice &qvoice)
{
    QVariantMap data;
    data["qvoice"] = QVariant::fromValue(qvoice);
    return {qvoice.name(), Language(qvoice.locale()), data};
}

static QVoice voiceToQVoice(const Voice &voice)
{
    const QVariantMap data = voice.data();
    if (data.contains("qvoice")) {
        return data.value("qvoice").value<QVoice>();
    }
    // If we can't convert, return an invalid QVoice
    return {};
}

QtTTSProvider::QtTTSProvider(QObject *parent)
    : ATTSProvider(parent)
    , m_tts(new QTextToSpeech(this))
{
    connect(m_tts, &QTextToSpeech::stateChanged, this, &QtTTSProvider::onStateChanged);
    connect(m_tts, &QTextToSpeech::errorOccurred, this, &QtTTSProvider::onErrorOccurred);
    connect(m_tts, &QTextToSpeech::sayingWord, this, &QtTTSProvider::onSayingWord);
}

QString QtTTSProvider::getProviderType() const
{
    return QStringLiteral("QtTTSProvider");
}

void QtTTSProvider::say(const QString &text)
{
    qDebug() << "QtTTSProvider::say - current locale:" << m_tts->locale().name() << "current voice:" << m_tts->voice().name();
    qDebug() << "QtTTSProvider::say - text length:" << text.length() << "characters";
    m_tts->say(text);
}

void QtTTSProvider::stop()
{
    m_tts->stop();
}

void QtTTSProvider::pause()
{
    m_tts->pause();
}

void QtTTSProvider::resume()
{
    m_tts->resume();
}

QTextToSpeech::State QtTTSProvider::state() const
{
    return m_tts->state();
}

QTextToSpeech::ErrorReason QtTTSProvider::errorReason() const
{
    return m_tts->errorReason();
}

QString QtTTSProvider::errorString() const
{
    return m_tts->errorString();
}

Language QtTTSProvider::language() const
{
    return Language(m_tts->locale());
}

void QtTTSProvider::setLanguage(const Language &language)
{
    QLocale locale = language.toQLocale();
    qDebug() << "QtTTSProvider::setLanguage - requested:" << language.displayName() << "(" << locale.bcp47Name() << ")";
    qDebug() << "QtTTSProvider::setLanguage - available languages:" << availableLanguages().size();

    // Try to find a better matching locale from available ones if exact match fails
    QLocale normalizedLocale = locale;
    const QList<Language> availableList = availableLanguages();
    bool exactMatch = false;

    for (const Language &availableLang : availableList) {
        if (availableLang.toQLocale() == locale) {
            exactMatch = true;
            break;
        }
    }

    if (!exactMatch) {
        qDebug() << "QtTTSProvider::setLanguage - exact locale not available, looking for fallbacks";

        // Try to find a locale with same language but different territory
        for (const Language &availableLang : availableList) {
            QLocale availableLocale = availableLang.toQLocale();
            if (availableLocale.language() == locale.language()) {
                qDebug() << "QtTTSProvider::setLanguage - found fallback locale:" << availableLocale.name() << "for requested:" << locale.name();
                normalizedLocale = availableLocale;
                break;
            }
        }
    }

    m_tts->setLocale(normalizedLocale);

    const QLocale actualLocale = m_tts->locale();
    qDebug() << "QtTTSProvider::setLanguage - actual locale set:" << actualLocale.name() << "(" << actualLocale.bcp47Name() << ")";

    // Check if we have voices for this locale
    const QList<Voice> voices = findVoices(Language(actualLocale));
    qDebug() << "QtTTSProvider::setLanguage - found" << voices.size() << "voices for actual locale" << actualLocale.name();
    for (const Voice &voice : voices) {
        qDebug() << "  - Voice:" << voice.name() << "for language:" << voice.language().displayName();
    }
}

Voice QtTTSProvider::voice() const
{
    return qvoiceToVoice(m_tts->voice());
}

void QtTTSProvider::setVoice(const Voice &voice)
{
    qDebug() << "QtTTSProvider::setVoice - requested voice:" << voice.name() << "for language:" << voice.language().displayName();

    const QVoice qvoice = voiceToQVoice(voice);
    if (qvoice.name().isEmpty()) {
        qDebug() << "QtTTSProvider::setVoice - ERROR: Could not convert Voice to QVoice";
        return;
    }

    m_tts->setVoice(qvoice);

    const QVoice actualVoice = m_tts->voice();
    qDebug() << "QtTTSProvider::setVoice - actual voice set:" << actualVoice.name() << "for locale:" << actualVoice.locale().name();
}

QList<Voice> QtTTSProvider::availableVoices() const
{
    QList<Voice> voices;
    const auto qvoices = m_tts->availableVoices();
    for (const QVoice &qvoice : qvoices) {
        voices.append(qvoiceToVoice(qvoice));
    }
    return voices;
}

QList<Voice> QtTTSProvider::findVoices(const Language &language) const
{
    QLocale locale = language.toQLocale();
    qDebug() << "QtTTSProvider::findVoices - searching for language:" << language.displayName() << "(" << locale.bcp47Name() << ")";

    QList<QVoice> qvoices = m_tts->findVoices(locale);
    qDebug() << "QtTTSProvider::findVoices - found" << qvoices.size() << "voices for exact locale match";

    // If no voices found for specific locale, try with just the language
    if (qvoices.isEmpty() && locale.territory() != QLocale::AnyTerritory) {
        qDebug() << "QtTTSProvider::findVoices - trying language-only fallback:" << QLocale::languageToString(locale.language());
        qvoices = m_tts->findVoices(locale.language());
        qDebug() << "QtTTSProvider::findVoices - found" << qvoices.size() << "voices for language fallback";
    }

    QList<Voice> voices;
    for (const QVoice &qvoice : qvoices) {
        qDebug() << "  - Found voice:" << qvoice.name() << "for locale:" << qvoice.locale().name() << "(" << qvoice.locale().bcp47Name() << ")";
        voices.append(qvoiceToVoice(qvoice));
    }

    if (voices.isEmpty()) {
        qDebug() << "QtTTSProvider::findVoices - WARNING: No voices found for language" << language.displayName();

#ifdef Q_OS_WIN
        // Show Windows-specific voice installation dialog
        showWindowsVoiceInstallationDialog(language);
#endif
    }

    return voices;
}

double QtTTSProvider::rate() const
{
    return m_tts->rate();
}

void QtTTSProvider::setRate(double rate)
{
    m_tts->setRate(rate);
}

double QtTTSProvider::pitch() const
{
    return m_tts->pitch();
}

void QtTTSProvider::setPitch(double pitch)
{
    m_tts->setPitch(pitch);
}

double QtTTSProvider::volume() const
{
    return m_tts->volume();
}

void QtTTSProvider::setVolume(double volume)
{
    m_tts->setVolume(volume);
}

QList<Language> QtTTSProvider::availableLanguages() const
{
    QList<Language> languages;
    const QList<QLocale> locales = m_tts->availableLocales();
    for (const QLocale &locale : locales) {
        languages.append(Language(locale));
    }
    return languages;
}

void QtTTSProvider::onStateChanged(QTextToSpeech::State state)
{
    emit stateChanged(state);
}

void QtTTSProvider::onErrorOccurred(QTextToSpeech::ErrorReason reason, const QString &errorString)
{
    emit errorOccurred(reason, errorString);
}

void QtTTSProvider::onSayingWord(const QString &word, qsizetype start, qsizetype length)
{
    emit sayingWord(word, start, length);
}

void QtTTSProvider::applyOptions(const ProviderOptions &options)
{
    Q_UNUSED(options);
    // Qt TTS provider has no configurable options currently
}

std::unique_ptr<ProviderOptions> QtTTSProvider::getDefaultOptions() const
{
    return std::make_unique<ProviderOptions>();
}

QStringList QtTTSProvider::getAvailableOptions() const
{
    return {}; // No options available for Qt TTS provider
}

ProviderUIRequirements QtTTSProvider::getUIRequirements() const
{
    ProviderUIRequirements requirements;
    requirements.requiredUIElements = {"sourceVoiceComboBox", "translationVoiceComboBox"};
    requirements.supportedSignals = {};
    requirements.supportedCapabilities = {"voiceSelection"};
    return requirements;
}

QStringList QtTTSProvider::availableSpeakers() const
{
    return {};
}

QStringList QtTTSProvider::availableSpeakersForVoice(const Voice &voice) const
{
    Q_UNUSED(voice);
    return availableSpeakers();
}

QString QtTTSProvider::currentSpeaker() const
{
    return {};
}

void QtTTSProvider::setSpeaker(const QString &speakerName)
{
    Q_UNUSED(speakerName)
}

#ifdef Q_OS_WIN
void QtTTSProvider::showWindowsVoiceInstallationDialog(const Language &language) const
{
    // Show dialog only once per language per session
    static QSet<QString> shownLanguages;
    const QString languageKey = language.toString();

    if (shownLanguages.contains(languageKey)) {
        return;
    }
    shownLanguages.insert(languageKey);

    const QString languageName = language.name();
    const QString dialogText = tr(
                                   "<h3>Windows TTS Voice Not Found</h3>"
                                   "<p>No Windows TTS voice was found for <b>%1</b>.</p>"
                                   "<p><b>To install additional languages:</b></p>"
                                   "<ol>"
                                   "<li>Open <b>Settings → Time & Language → Language & region</b></li>"
                                   "<li>Click <b>Add a language</b> and select your desired language</li>"
                                   "<li>Click on the installed language and select <b>Language options</b></li>"
                                   "<li>Under <b>Speech</b>, click <b>Download</b> for the speech pack</li>"
                                   "</ol>"
                                   "<p><b>⚠️ License Note:</b> Crow Translate currently only supports \"Basic voice models\" from Windows TTS. "
                                   "\"Narrator voices\" have licensing restrictions that are incompatible with this application.</p>")
                                   .arg(languageName);

    QMessageBox msgBox(QMessageBox::Information, tr("Windows TTS Voice Installation"), dialogText);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}
#endif
