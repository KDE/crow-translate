/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cli.h"

#include "instancepinger.h"
#include "provideroptions.h"
#include "provideroptionsmanager.h"
#include "settings/appsettings.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QRegularExpression>
#include <QTimer>

#include <cstdlib>

Cli::Cli(QObject *parent)
    : QObject(parent)
{
}

void Cli::process(const QCoreApplication &app)
{
    AppSettings settings;

    const QCommandLineOption codes({"c", "codes"}, tr("Display all language codes."));
    const QCommandLineOption source({"s", "source"},
                                    tr("Specify the source language (by default, engine will try to determine the language on its own)."),
                                    QStringLiteral("code"),
                                    QStringLiteral("auto"));
    const QCommandLineOption translation({"t", "translation"},
                                         tr("Specify the translation language(s), split by '+' (by default, the system language is used)."),
                                         QStringLiteral("code"),
                                         QStringLiteral("auto"));
    const QCommandLineOption engine({"e", "engine"},
                                    tr("Specify the translator engine ('google', 'yandex', 'bing', 'libretranslate' or 'lingva'), Google is used by default."),
                                    QStringLiteral("engine"),
                                    QStringLiteral("google"));
    const QCommandLineOption url({"u", "url"},
                                 tr("Specify Mozhi instance URL. Instance URL from the app settings will be used by default."),
                                 QStringLiteral("URL"),
                                 settings.instance());
    const QCommandLineOption translationProvider({"tp", "translation-provider"},
                                                 tr("Specify translation provider ('copy' or 'mozhi'). Provider from app settings will be used by default."),
                                                 QStringLiteral("provider"));
    const QCommandLineOption ttsProvider({"tts", "tts-provider"},
                                         tr("Specify TTS provider ('none', 'mozhi', 'qt', or 'piper'). Provider from app settings will be used by default."),
                                         QStringLiteral("provider"));
    const QCommandLineOption speakTranslation({"r", "speak-translation"}, tr("Speak the translation."));
    const QCommandLineOption speakSource({"o", "speak-source"}, tr("Speak the source."));
    const QCommandLineOption file({"f", "file"}, tr("Read source text from files. Arguments will be interpreted as file paths."));
    const QCommandLineOption readStdin({"i", "stdin"}, tr("Add stdin data to source text."));
    const QCommandLineOption audioOnly({"a", "audio-only"},
                                       tr("Do not print any text when using --%1 or --%2.").arg(speakSource.names().at(1), speakTranslation.names().at(1)));
    const QCommandLineOption brief({"b", "brief"}, tr("Print only translations."));
    const QCommandLineOption json({"j", "json"}, tr("Print output formatted as JSON."));

    QCommandLineParser parser;
    parser.setApplicationDescription(tr("Application that allows to translate and speak text using various providers"));
    parser.addPositionalArgument(QStringLiteral("text"), tr("Text to translate. By default, the translation will be done to the system language."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(codes);
    parser.addOption(source);
    parser.addOption(translation);
    parser.addOption(engine);
    parser.addOption(url);
    parser.addOption(translationProvider);
    parser.addOption(ttsProvider);
    parser.addOption(speakTranslation);
    parser.addOption(speakSource);
    parser.addOption(file);
    parser.addOption(readStdin);
    parser.addOption(audioOnly);
    parser.addOption(brief);
    parser.addOption(json);
    parser.process(app);

    checkIncompatibleOptions(parser, audioOnly, brief);
    checkIncompatibleOptions(parser, json, audioOnly);
    checkIncompatibleOptions(parser, json, brief);

    if (parser.isSet(audioOnly) && !parser.isSet(speakSource) && !parser.isSet(speakTranslation)) {
        qCritical() << tr("Error: For --%1 you must specify --%2 and/or --%3 options")
                           .arg(audioOnly.names().at(1), speakSource.names().at(1), speakTranslation.names().at(1))
                    << '\n';
        parser.showHelp(1);
    }

    // Only show language codes
    if (parser.isSet(codes)) {
        printLangCodes();
        QCoreApplication::quit();
        return;
    }

    // Source language
    const QString sourceLangCode = parser.value(source);
    if (sourceLangCode == "auto") {
        m_sourceLang = Language::autoLanguage(); // Use auto marker
    } else {
        m_sourceLang = Language(sourceLangCode);
        if (m_sourceLang == Language::autoLanguage()) {
            qCritical() << tr("Error: Unknown source language code '%1'").arg(sourceLangCode) << '\n';
            parser.showHelp(1);
        }
    }

    // Translation languages
    const QString translationValue = parser.value(translation);
    if (translationValue == "auto") {
        m_translationLanguages.append(Language(QLocale::system()));
    } else {
        for (const QString &langCode : translationValue.split('+')) {
            const Language language = Language(langCode);
            if (language == Language::autoLanguage()) {
                qCritical() << tr("Error: Unknown translation language code '%1'").arg(langCode) << '\n';
                parser.showHelp(1);
            }
            m_translationLanguages.append(language);
        }
    }

    // Source text
    if (parser.isSet(file)) {
        if (parser.isSet(readStdin))
            m_sourceText += readFilesFromStdin();

        m_sourceText += readFilesFromArguments(parser.positionalArguments());
    } else {
        if (parser.isSet(readStdin))
            m_sourceText += QTextStream(stdin).readAll();

        m_sourceText += parser.positionalArguments().join(' ');
    }

    if (m_sourceText.endsWith('\n'))
        m_sourceText.chop(1);

    if (m_sourceText.isEmpty()) {
        qCritical() << tr("Error: There is no text for translation") << '\n';
        parser.showHelp(1);
    }

    // Initialize translation provider - determine backend from CLI or settings
    ATranslationProvider::ProviderBackend translationBackend = settings.translationProviderBackend();
    if (parser.isSet(translationProvider)) {
        const QString providerName = parser.value(translationProvider).toLower();
        if (providerName == "copy") {
            translationBackend = ATranslationProvider::ProviderBackend::Copy;
        } else if (providerName == "mozhi") {
            translationBackend = ATranslationProvider::ProviderBackend::Mozhi;
        } else {
            qCritical() << tr("Error: Unknown translation provider '%1'").arg(providerName) << '\n';
            parser.showHelp(1);
        }
    }

    m_translator = ATranslationProvider::createTranslationProvider(this, translationBackend);
    connect(m_translator, &ATranslationProvider::stateChanged, this, &Cli::onTranslationStateChanged);

    // Set up Mozhi instance for auto-detection (only if no URL specified)
    if (!parser.isSet(url) && settings.instance().isEmpty()) {
        qInfo() << tr("Detecting fastest instance");

        InstancePinger pinger;
        QEventLoop loop;
        connect(&pinger, &InstancePinger::finished, &loop, &QEventLoop::quit);
        pinger.detectFastest();
        loop.exec();

        settings.setInstance(pinger.fastestInstance());
    }

    // Apply saved settings first
    ProviderOptionsManager optionsManager;
    optionsManager.applySettingsToTranslationProvider(m_translator);

    // Override with CLI arguments if provided
    if (m_translator->getProviderType() == "MozhiTranslationProvider") {
        auto options = std::make_unique<ProviderOptions>();
        bool hasOverrides = false;

        // Override instance if specified
        if (parser.isSet(url)) {
            options->setOption("instance", parser.value(url));
            hasOverrides = true;
        }

        // Override engine if specified
        if (parser.isSet(engine)) {
            const QString engineName = parser.value(engine);
            int engineValue = -1;
            if (engineName == "google") {
                engineValue = static_cast<int>(OnlineTranslator::Google);
            } else if (engineName == "yandex") {
                engineValue = static_cast<int>(OnlineTranslator::Yandex);
            } else if (engineName == "bing" || engineName == "duckduckgo") {
                engineValue = static_cast<int>(OnlineTranslator::Duckduckgo);
            } else if (engineName == "libretranslate") {
                engineValue = static_cast<int>(OnlineTranslator::LibreTranslate);
            } else if (engineName == "lingva") {
                qCritical() << tr("Error: Lingva engine is not supported") << '\n';
                parser.showHelp(1);
            } else if (engineName == "mymemory") {
                engineValue = static_cast<int>(OnlineTranslator::Mymemory);
            } else if (engineName == "reverso") {
                engineValue = static_cast<int>(OnlineTranslator::Reverso);
            } else if (engineName == "deepl") {
                engineValue = static_cast<int>(OnlineTranslator::Deepl);
            } else {
                qCritical() << tr("Error: Unknown engine") << '\n';
                parser.showHelp(1);
            }

            if (engineValue != -1) {
                options->setOption("engine", engineValue);
                hasOverrides = true;
            }
        }

        // Apply CLI overrides if any
        if (hasOverrides) {
            m_translator->applyOptions(*options);
        }
    }

    // Initialize TTS provider if needed
    if (parser.isSet(speakSource) || parser.isSet(speakTranslation)) {
        qDebug() << "Initializing TTS provider";

        // Determine TTS backend from CLI or settings
        ATTSProvider::ProviderBackend ttsBackend = settings.ttsProviderBackend();
        if (parser.isSet(ttsProvider)) {
            const QString providerName = parser.value(ttsProvider).toLower();
            if (providerName == "none") {
                ttsBackend = ATTSProvider::ProviderBackend::None;
            } else if (providerName == "mozhi") {
                ttsBackend = ATTSProvider::ProviderBackend::Mozhi;
            } else if (providerName == "qt") {
                ttsBackend = ATTSProvider::ProviderBackend::Qt;
            } else if (providerName == "piper") {
                ttsBackend = ATTSProvider::ProviderBackend::Piper;
            } else {
                qCritical() << tr("Error: Unknown TTS provider '%1'").arg(providerName) << '\n';
                parser.showHelp(1);
            }
        }

        m_tts = ATTSProvider::createTTSProvider(this, ttsBackend);
        qDebug() << "Using TTS provider:" << static_cast<int>(ttsBackend);

        connect(m_tts, &ATTSProvider::stateChanged, this, &Cli::onTTSStateChanged);

        // Apply saved settings first
        ProviderOptionsManager optionsManager;
        optionsManager.applySettingsToTTSProvider(m_tts);

        // Override with CLI arguments if provided for Mozhi TTS
        if (m_tts->getProviderType() == "MozhiTTSProvider") {
            auto ttsOptions = std::make_unique<ProviderOptions>();
            bool hasTTSOverrides = false;

            // Override instance if specified
            if (parser.isSet(url)) {
                ttsOptions->setOption("instance", parser.value(url));
                hasTTSOverrides = true;
            }

            // Override engine if specified
            if (parser.isSet(engine)) {
                const QString engineName = parser.value(engine);
                int engineValue = -1;
                if (engineName == "google") {
                    engineValue = static_cast<int>(OnlineTranslator::Google);
                } else if (engineName == "yandex") {
                    engineValue = static_cast<int>(OnlineTranslator::Yandex);
                } else if (engineName == "bing" || engineName == "duckduckgo") {
                    engineValue = static_cast<int>(OnlineTranslator::Duckduckgo);
                } else if (engineName == "libretranslate") {
                    engineValue = static_cast<int>(OnlineTranslator::LibreTranslate);
                } else if (engineName == "mymemory") {
                    engineValue = static_cast<int>(OnlineTranslator::Mymemory);
                } else if (engineName == "reverso") {
                    engineValue = static_cast<int>(OnlineTranslator::Reverso);
                } else if (engineName == "deepl") {
                    engineValue = static_cast<int>(OnlineTranslator::Deepl);
                }

                if (engineValue != -1) {
                    ttsOptions->setOption("engine", engineValue);
                    hasTTSOverrides = true;
                }
            }

            // Apply CLI overrides if any
            if (hasTTSOverrides) {
                m_tts->applyOptions(*ttsOptions);
            }
        }
    }

    // Audio options
    m_speakSource = parser.isSet(speakSource);
    m_speakTranslation = parser.isSet(speakTranslation);

    // Modes
    m_audioOnly = parser.isSet(audioOnly);
    m_brief = parser.isSet(brief);
    m_json = parser.isSet(json);

    // Start translation process
    processNextTranslation();
}

void Cli::onTranslationStateChanged(ATranslationProvider::State state)
{
    qDebug() << "CLI: Translation state changed to:" << static_cast<int>(state) << "Error:" << static_cast<int>(m_translator->error);
    if (state == ATranslationProvider::State::Processed) {
        // Check for translation error
        if (m_translator->error != ATranslationProvider::TranslationError::NoError) {
            const QString errorString = m_translator->getErrorString();
            if (!errorString.isEmpty()) {
                qCritical() << tr("Error: %1").arg(errorString);
            } else {
                qCritical() << tr("Translation error occurred");
            }
            cleanup();
            QCoreApplication::exit(1);
            return;
        }

        // Translation successful
        m_currentTranslationResult = m_translator->result;

        // Update source language with detected language if auto-detection was used
        if (m_sourceLang == Language::autoLanguage() && m_translator->sourceLanguage != Language::autoLanguage()) {
            m_sourceLang = m_translator->sourceLanguage;
            qDebug() << "Auto-detected source language:" << m_sourceLang.name();
        }

        m_translator->finish();

        if (!m_audioOnly) {
            printTranslation();
        }

        // Handle TTS - only start if no TTS is currently active
        if (m_ttsState == TTSState::None) {
            if (m_speakSource) {
                m_ttsState = TTSState::SpeakingSource;
                m_waitingForTTS = true;
                speakText(m_sourceText, m_sourceLang);
                return;
            }
            if (m_speakTranslation) {
                m_ttsState = TTSState::SpeakingTranslation;
                m_waitingForTTS = true;
                speakText(m_currentTranslationResult, m_currentTargetLang);
                return;
            }
        }

        // No TTS needed, move to next translation
        m_currentTranslationIndex++;
        m_ttsState = TTSState::None;
        processNextTranslation();
    } else if (state == ATranslationProvider::State::Finished) {
        // Check for translation error
        if (m_translator->error != ATranslationProvider::TranslationError::NoError) {
            const QString errorString = m_translator->getErrorString();
            if (!errorString.isEmpty()) {
                qCritical() << tr("Error: %1").arg(errorString);
            } else {
                qCritical() << tr("Translation error occurred");
            }
            cleanup();
            std::exit(1);
        }

        // Reset for next translation
        m_translator->reset();
    }
}

void Cli::onTTSStateChanged(QTextToSpeech::State state)
{
    qDebug() << "TTS state changed to:" << state << "Current TTS state:" << static_cast<int>(m_ttsState);
    qDebug() << "m_speakSource:" << m_speakSource << "m_speakTranslation:" << m_speakTranslation;

    if (state == QTextToSpeech::Ready) {
        if (m_ttsState == TTSState::SpeakingSource && m_speakTranslation) {
            qDebug() << "Transitioning from source to translation speech";
            qDebug() << "Translation text:" << m_currentTranslationResult;
            qDebug() << "Target language:" << m_currentTargetLang.name();
            m_ttsState = TTSState::SpeakingTranslation;

            // Add validation before calling speakText
            if (m_currentTranslationResult.isEmpty()) {
                qWarning() << "Translation result is empty, skipping translation speech";
                m_waitingForTTS = false;
                m_ttsState = TTSState::None;
                m_currentTranslationIndex++;
                processNextTranslation();
                return;
            }

            if (m_tts == nullptr) {
                qWarning() << "TTS provider is null during transition";
                m_waitingForTTS = false;
                m_ttsState = TTSState::None;
                m_currentTranslationIndex++;
                processNextTranslation();
                return;
            }

            qDebug() << "About to call speakText for translation";
            speakText(m_currentTranslationResult, m_currentTargetLang);
            qDebug() << "Called speakText for translation";
            return;
        }

        qDebug() << "TTS finished, resetting state and moving to next translation";
        m_waitingForTTS = false;
        m_ttsState = TTSState::None;

        // Move to next translation
        m_currentTranslationIndex++;
        processNextTranslation();
    } else if (state == QTextToSpeech::Error) {
        const QString errorString = (m_tts != nullptr) ? m_tts->errorString() : "Unknown error";
        qCritical() << tr("Error: TTS failed") << errorString;
        m_waitingForTTS = false;
        m_ttsState = TTSState::None;

        // Continue with next translation
        m_currentTranslationIndex++;
        processNextTranslation();
    } else {
        qDebug() << "TTS state changed to unhandled state:" << state;
    }
}

void Cli::translateText(const QString &text, const Language &sourceLang, const Language &targetLang)
{
    m_currentTargetLang = targetLang;
    m_translator->translate(text, targetLang, sourceLang);
}

void Cli::processNextTranslation()
{
    if (m_currentTranslationIndex >= m_translationLanguages.size()) {
        cleanup();
        QCoreApplication::quit();
        return;
    }

    const Language targetLang = m_translationLanguages[m_currentTranslationIndex];
    translateText(m_sourceText, m_sourceLang, targetLang);
}

void Cli::printTranslation()
{
    // JSON mode
    if (m_json) {
        // For JSON, we'd need to create a JSON structure
        // This is simplified for now
        QJsonDocument doc;
        QJsonObject obj;
        obj["source"] = m_sourceText;
        obj["translation"] = m_currentTranslationResult;
        obj["source_language"] = m_sourceLang.name();
        obj["target_language"] = m_currentTargetLang.name();
        doc.setObject(obj);
        m_stdout << doc.toJson();
        return;
    }

    // Short mode
    if (m_brief) {
        m_stdout << m_currentTranslationResult << Qt::endl;
        return;
    }

    // Show source text only once
    if (!m_sourcePrinted) {
        m_stdout << m_sourceText << '\n';
        m_sourcePrinted = true;
    }
    m_stdout << '\n';

    // Languages
    m_stdout << "[ " << m_sourceLang.name() << " -> ";
    m_stdout << m_currentTargetLang.name() << " ]\n\n";

    // Translation
    if (!m_currentTranslationResult.isEmpty()) {
        m_stdout << m_currentTranslationResult << '\n';
        m_stdout << '\n';
    }

    m_stdout.flush();
}

void Cli::speakText(const QString &text, const Language &language)
{
    qDebug() << "=== ENTERED speakText ===";
    qDebug() << "Text:" << text;
    qDebug() << "Language:" << language.name();

    if (m_tts == nullptr) {
        qWarning() << "TTS provider not initialized";
        return;
    }

    if (text.isEmpty()) {
        qWarning() << "Cannot speak empty text";
        return;
    }

    qDebug() << "TTS: Speaking text:" << text << "with language:" << language.name();
    qDebug() << "TTS: Current state before speaking:" << m_tts->state();

    // Find the best available locale for TTS
    const Language bestLanguage = findBestTTSLanguage(language);
    qDebug() << "TTS: Using best locale:" << bestLanguage.name();

    // Set locale and find appropriate voice
    m_tts->setLanguage(bestLanguage);
    QList<Voice> availableVoices = m_tts->findVoices(bestLanguage);
    if (!availableVoices.isEmpty()) {
        m_tts->setVoice(availableVoices.first());
        qDebug() << "TTS: Selected voice:" << availableVoices.first().name() << "model path:" << availableVoices.first().modelPath();
    } else {
        qDebug() << "TTS: No voices found for locale, using current voice";
    }

    if (m_tts->state() == QTextToSpeech::Ready) {
        qDebug() << "TTS: Calling say() directly";
        m_tts->say(text);
        qDebug() << "TTS: say() call completed";
    } else {
        qDebug() << "TTS: Waiting for Ready state before calling say(), current state:" << m_tts->state();
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(m_tts, &ATTSProvider::stateChanged, this, [this, text, connection](QTextToSpeech::State state) {
            if (state == QTextToSpeech::Ready) {
                qDebug() << "TTS: Ready state reached, calling say()";
                disconnect(*connection);
                m_tts->say(text);
                qDebug() << "TTS: say() call completed from wait";
            }
        });
    }
    qDebug() << "TTS: speakText() method completed";
}

void Cli::printLangCodes()
{
    // Print common language codes
    QList<QLocale::Language> languages;
    languages << QLocale::English << QLocale::Spanish << QLocale::French << QLocale::German << QLocale::Italian << QLocale::Portuguese << QLocale::Russian
              << QLocale::Chinese << QLocale::Japanese << QLocale::Korean << QLocale::Arabic << QLocale::Hindi << QLocale::Dutch << QLocale::Swedish
              << QLocale::NorwegianBokmal << QLocale::Danish << QLocale::Finnish << QLocale::Polish << QLocale::Czech << QLocale::Hungarian << QLocale::Romanian
              << QLocale::Bulgarian << QLocale::Greek << QLocale::Turkish << QLocale::Hebrew << QLocale::Thai << QLocale::Vietnamese << QLocale::Ukrainian
              << QLocale::Croatian << QLocale::Slovak << QLocale::Slovenian << QLocale::Estonian << QLocale::Latvian << QLocale::Lithuanian;

    for (const auto &lang : languages) {
        const Language language = Language(QLocale(lang));
        m_stdout << QLocale::languageToString(lang) << " - " << language.toCode() << '\n';
    }
}

void Cli::cleanup()
{
    if (m_translator != nullptr) {
        m_translator->deleteLater();
        m_translator = nullptr;
    }

    if (m_tts != nullptr) {
        m_tts->deleteLater();
        m_tts = nullptr;
    }
}

void Cli::checkIncompatibleOptions(QCommandLineParser &parser, const QCommandLineOption &option1, const QCommandLineOption &option2)
{
    if (parser.isSet(option1) && parser.isSet(option2)) {
        qCritical() << tr("Error: You can't use --%1 with --%2").arg(option1.names().at(1), option2.names().at(1)) << '\n';
        parser.showHelp(1);
    }
}

QByteArray Cli::readFilesFromStdin()
{
    const QString stdinText = QTextStream(stdin).readAll();
    QByteArray filesData;
    for (const QString &filePath : stdinText.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)) {
        QFile file(filePath);
        if (!file.exists()) {
            qCritical() << tr("Error: File does not exist: %1").arg(file.fileName());
            continue;
        }

        if (!file.open(QFile::ReadOnly)) {
            qCritical() << tr("Error: Unable to open file: %1").arg(file.fileName());
            continue;
        }

        filesData += file.readAll();
    }

    return filesData;
}

QByteArray Cli::readFilesFromArguments(const QStringList &arguments)
{
    QByteArray filesData;
    for (const QString &filePath : arguments) {
        QFile file(filePath);
        if (!file.exists()) {
            qCritical() << tr("Error: File does not exist: %1").arg(file.fileName());
            continue;
        }

        if (!file.open(QFile::ReadOnly)) {
            qCritical() << tr("Error: Unable to open file: %1").arg(file.fileName());
            continue;
        }

        filesData += file.readAll();
    }

    return filesData;
}

Language Cli::findBestTTSLanguage(const Language &requestedLanguage)
{
    qDebug() << "findBestTTSLanguage called with:" << requestedLanguage.name();

    if (m_tts == nullptr) {
        qDebug() << "findBestTTSLocale: TTS provider is null";
        return Language(QLocale::system());
    }

    qDebug() << "findBestTTSLocale: Getting available locales...";
    // Get all available locales from TTS
    QList<Language> availableLanguages = m_tts->availableLanguages();
    qDebug() << "findBestTTSLanguage: Got" << availableLanguages.size() << "available languages";

    // First try: exact match
    for (const Language &available : availableLanguages) {
        if (available == requestedLanguage) {
            return available;
        }
    }

    // Second try: same language, different country
    for (const Language &available : availableLanguages) {
        if (available.hasQLocaleEquivalent() && requestedLanguage.hasQLocaleEquivalent() && available.toQLocale().language() == requestedLanguage.toQLocale().language()) {
            return available;
        }
    }

    // Third try: if requested locale is C (auto), try system locale
    if (requestedLanguage == Language::autoLanguage()) {
        const QLocale systemLocale = QLocale::system();
        for (const Language &available : availableLanguages) {
            if (available.hasQLocaleEquivalent() && available.toQLocale().language() == systemLocale.language()) {
                return available;
            }
        }
    }

    // Fallback: return first available language or system language
    if (!availableLanguages.isEmpty()) {
        return availableLanguages.first();
    }

    return Language(QLocale::system());
}
