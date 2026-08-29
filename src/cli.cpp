/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cli.h"

#include "instancepinger.h"
#include "provideroptions.h"
#include "core/translationsession.h"
#include "core/usernotifier.h"
#include "frontend/frontendregistry.h"
#include "ocr/aocrprovider.h"
#include "settings/appsettings.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QRegularExpression>
#include <QTimer>

#include <cstdlib>

namespace
{
// User-facing messages must not go through qCritical()/qInfo(). Qt built with
// journald support - which several distributions enable - routes categorised
// logging to the journal rather than stderr, so on those builds every reason
// the CLI gave for refusing to do something went to the system log and never
// appeared in the terminal that asked for it. The user saw a bare usage
// message, or nothing at all. QCommandLineParser writes its own errors
// straight to stderr for the same reason.
//
// Diagnostics go to stderr, never stdout: stdout carries the translation, and
// a caller redirecting it must not have anything else mixed in.
QTextStream &errorStream()
{
    static QTextStream stream(stderr);
    return stream;
}

void printError(const QString &message)
{
    errorStream() << message << Qt::endl;
}

// Renders what a backend found for a terminal. The transliterations and the
// dictionary entries keep the shape they had in the window - /like this/ for
// the translation's romanisation, (like this) for the source's, [like this]
// for the transcription - without any of the markup that carried it there.
//
// This replaces a function that took the backend's HTML apart again with
// regular expressions. Nothing renders and un-renders any more: the backend
// hands over fields and this writes them out.
QString resultToPlainText(const TranslationResult &result)
{
    QString text = result.translation;

    if (!result.translationTranslit.isEmpty())
        text += QStringLiteral("\n/%1/").arg(result.translationTranslit);

    if (!result.sourceTranslit.isEmpty())
        text += QStringLiteral("\n(%1)").arg(result.sourceTranslit);

    if (!result.sourceTranscription.isEmpty())
        text += QStringLiteral("\n[%1]").arg(result.sourceTranscription);

    if (!result.options.isEmpty()) {
        text += QStringLiteral("\n\n") + QCoreApplication::translate("Cli", "translation options:");
        for (const auto &[word, translations] : result.options) {
            text += QStringLiteral("\n    ") + word;
            if (!translations.isEmpty())
                text += QStringLiteral(": ") + translations.join(QStringLiteral(", "));
        }
    }

    if (!result.examples.isEmpty()) {
        text += QStringLiteral("\n\n") + QCoreApplication::translate("Cli", "examples:");
        for (const auto &[word, definition, example, examplesSource, examplesTarget] : result.examples) {
            text += QStringLiteral("\n    ") + word;
            if (!definition.isEmpty())
                text += QStringLiteral("\n    ") + definition;
            if (!example.isEmpty())
                text += QStringLiteral("\n    ") + example;
            for (qsizetype i = 0; i < examplesSource.size(); ++i)
                text += QStringLiteral("\n    %1 %2").arg(examplesSource[i], examplesTarget.value(i));
        }
    }

    return text;
}
} // namespace

Cli::Cli(QObject *parent)
    : QObject(parent)
    , m_session(new TranslationSession(this))
{
}

void Cli::process(const QCoreApplication &app)
{
    AppSettings settings;

    // These used to be QMessageBox::exec() calls inside the providers, so in a
    // CLI run they constructed dialogs that no one would ever see - a missing
    // voice model or a provider that failed to start simply produced silence.
    connect(UserNotifier::instance(), &UserNotifier::notified, this, &Cli::printNotification);

    const QCommandLineOption codes({"c", "codes"}, tr("Display all language codes."));
    const QCommandLineOption source({"s", "source"},
                                    tr("Specify the source language (by default, engine will try to determine the language on its own)."),
                                    QStringLiteral("code"),
                                    QStringLiteral("auto"));
    const QCommandLineOption translation({"t", "translation"},
                                         tr("Specify the translation language(s), split by '+' (by default, the system language is used)."),
                                         QStringLiteral("code"),
                                         QStringLiteral("auto"));
    const QCommandLineOption engine(
        {"e", "engine"},
        tr("Specify the translator engine ('google', 'yandex', 'bing', 'deepl', 'libretranslate', 'mymemory' or 'reverso'), Google is used by default."),
        QStringLiteral("engine"),
        QStringLiteral("google"));
    const QCommandLineOption url({"u", "url"},
                                 tr("Specify Mozhi instance URL. Instance URL from the app settings will be used by default."),
                                 QStringLiteral("URL"),
                                 settings.instance());
    const QCommandLineOption translationProvider({"tp", "translation-provider"},
                                                 tr("Specify translation provider ('copy', 'mozhi' or 'localai'). Provider from app settings will be used by default."),
                                                 QStringLiteral("provider"));
    // Built without WITH_TTS there is nothing but 'none' to choose, and
    // without WITH_PIPER_TTS 'piper' is rejected by the parsing below - so
    // the text has to be guarded the same way the parsing is, or the binary
    // advertises providers it then refuses.
    const QCommandLineOption ttsProvider({"tts", "tts-provider"},
#if defined(WITH_PIPER_TTS)
                                         tr("Specify TTS provider ('none', 'mozhi', 'qt' or 'piper'). Provider from app settings will be used by default."),
#elif defined(WITH_TTS)
                                         tr("Specify TTS provider ('none', 'mozhi' or 'qt'). Provider from app settings will be used by default."),
#else
                                         tr("Specify TTS provider ('none'). This build has text-to-speech disabled."),
#endif
                                         QStringLiteral("provider"));
    const QCommandLineOption speakTranslation({"r", "speak-translation"}, tr("Speak the translation."));
    const QCommandLineOption speakSource({"o", "speak-source"}, tr("Speak the source."));
    const QCommandLineOption file({"f", "file"}, tr("Read source text from files. Arguments will be interpreted as file paths."));
    const QCommandLineOption readStdin({"i", "stdin"}, tr("Add stdin data to source text."));
    const QCommandLineOption image(QStringList{QStringLiteral("image")},
                                   tr("Read source text from an image using the configured OCR engine."),
                                   tr("path"));
    const QCommandLineOption audioOnly({"a", "audio-only"},
                                       tr("Do not print any text when using --%1 or --%2.").arg(longOptionName(speakSource), longOptionName(speakTranslation)));
    const QCommandLineOption brief({"b", "brief"}, tr("Print only translations."));
    const QCommandLineOption json({"j", "json"}, tr("Print output formatted as JSON."));
    // Declared so it is accepted and documented here, though main() has
    // already acted on it - the frontend has to be chosen before there is a
    // QCoreApplication for this parser to exist under. Without declaring it,
    // the parser would reject the very option that selected this frontend.
    const QCommandLineOption frontend(FrontendRegistry::frontendOptionName(),
                                      tr("Which frontend to run ('gui' or 'cli'). Defaults to 'cli' whenever any argument is given."),
                                      tr("frontend"),
                                      QStringLiteral("cli"));

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
    parser.addOption(image);
    parser.addOption(audioOnly);
    parser.addOption(brief);
    parser.addOption(json);
    parser.addOption(frontend);
    parser.process(app);

    checkIncompatibleOptions(parser, image, file);
    checkIncompatibleOptions(parser, image, readStdin);
    checkIncompatibleOptions(parser, audioOnly, brief);
    checkIncompatibleOptions(parser, json, audioOnly);
    checkIncompatibleOptions(parser, json, brief);

    if (parser.isSet(audioOnly) && !parser.isSet(speakSource) && !parser.isSet(speakTranslation)) {
        exitWithUsage(parser,
                      tr("Error: For --%1 you must specify --%2 and/or --%3 options")
                          .arg(longOptionName(audioOnly), longOptionName(speakSource), longOptionName(speakTranslation)));
    }

    // Resolve the engine name once, before anything uses it: the translation
    // provider and the TTS provider both take it, and they must agree.
    OnlineTranslator::Engine selectedEngine = OnlineTranslator::Google;
    if (parser.isSet(engine)) {
        const std::optional<OnlineTranslator::Engine> resolved = engineFromName(parser.value(engine));
        if (!resolved.has_value())
            exitWithUsage(parser, tr("Error: Unknown engine '%1'").arg(parser.value(engine)));
        selectedEngine = *resolved;
    }

    // Initialize translation provider - determine backend from CLI or settings
    ATranslationProvider::ProviderBackend translationBackend = settings.translationProviderBackend();
    if (parser.isSet(translationProvider)) {
        const QString providerName = parser.value(translationProvider).toLower();
        if (providerName == "copy") {
            translationBackend = ATranslationProvider::ProviderBackend::Copy;
        } else if (providerName == "mozhi") {
            translationBackend = ATranslationProvider::ProviderBackend::Mozhi;
        } else if (providerName == "localai" || providerName == "ollama") {
            translationBackend = ATranslationProvider::ProviderBackend::LocalAI;
        } else {
            exitWithUsage(parser, tr("Error: Unknown translation provider '%1'").arg(providerName));
        }
    }

    m_session->setTranslationBackend(translationBackend);
    connect(m_session, &TranslationSession::translationStateChanged, this, &Cli::onTranslationStateChanged);

    // Only show language codes. Deliberately placed after the provider exists
    // and before anything needs an instance: the list comes from the provider,
    // but producing it makes no request.
    if (parser.isSet(codes)) {
        printLangCodes();
        cleanup();
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
            exitWithUsage(parser, tr("Error: Unknown source language code '%1'").arg(sourceLangCode));
        }
    }

    // Translation languages
    const QString translationValue = parser.value(translation);
    if (translationValue == "auto") {
        // Asked of the shared core rather than worked out here, so "auto"
        // cannot mean two different things depending on which frontend asked.
        // The CLI used to take the system locale unconditionally and ignore
        // the configured primary/secondary pair, so `crow -s en text` on an
        // English system asked for English to English and got the source back
        // - while the window, given the same settings and the same text,
        // translated it into the primary language.
        m_translationLanguages.append(m_session->preferredDestination(m_sourceLang));
    } else {
        for (const QString &langCode : translationValue.split('+')) {
            const Language language = Language(langCode);
            if (language == Language::autoLanguage()) {
                exitWithUsage(parser, tr("Error: Unknown translation language code '%1'").arg(langCode));
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

    m_imagePath = parser.value(image);
    if (m_sourceText.isEmpty() && m_imagePath.isEmpty()) {
        exitWithUsage(parser, tr("Error: There is no text for translation"));
    }
    if (!m_sourceText.isEmpty() && !m_imagePath.isEmpty()) {
        exitWithUsage(parser, tr("Error: --%1 cannot be combined with text arguments").arg(longOptionName(image)));
    }

    // Set up Mozhi instance for auto-detection (only if no URL specified)
    if (!parser.isSet(url) && settings.instance().isEmpty()) {
        printError(tr("Detecting fastest instance"));

        InstancePinger pinger;
        QEventLoop loop;
        connect(&pinger, &InstancePinger::finished, &loop, &QEventLoop::quit);
        pinger.detectFastest();
        loop.exec();

        settings.setInstance(pinger.fastestInstance());
    }

    // Re-applied rather than relied on from the backend switch above: the
    // instance may have been detected and stored since, and the provider was
    // filled in before that happened.
    m_session->applyTranslationOptions();

    // Override with CLI arguments if provided
    if (m_session->translator()->getProviderType() == "MozhiTranslationProvider") {
        auto options = std::make_unique<ProviderOptions>();
        bool hasOverrides = false;

        // Override instance if specified
        if (parser.isSet(url)) {
            options->setOption("instance", parser.value(url));
            hasOverrides = true;
        }

        // Override engine if specified
        if (parser.isSet(engine)) {
            options->setOption("engine", static_cast<int>(selectedEngine));
            hasOverrides = true;
        }

        // Apply CLI overrides if any
        if (hasOverrides) {
            m_session->translator()->applyOptions(*options);
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
            }
#ifdef WITH_TTS
            else if (providerName == "mozhi") {
                ttsBackend = ATTSProvider::ProviderBackend::Mozhi;
            } else if (providerName == "qt") {
                ttsBackend = ATTSProvider::ProviderBackend::Qt;
            }
#endif
#ifdef WITH_PIPER_TTS
            else if (providerName == "piper") {
                ttsBackend = ATTSProvider::ProviderBackend::Piper;
            }
#endif
            else {
                exitWithUsage(parser, tr("Error: Unknown TTS provider '%1'").arg(providerName));
            }
        }

        m_session->setTtsBackend(ttsBackend);
        qDebug() << "Using TTS provider:" << static_cast<int>(ttsBackend);

        connect(m_session, &TranslationSession::ttsStateChanged, this, &Cli::onTTSStateChanged);

        // Override with CLI arguments if provided for Mozhi TTS
        if (m_session->tts()->getProviderType() == "MozhiTTSProvider") {
            auto ttsOptions = std::make_unique<ProviderOptions>();
            bool hasTTSOverrides = false;

            // Override instance if specified
            if (parser.isSet(url)) {
                ttsOptions->setOption("instance", parser.value(url));
                hasTTSOverrides = true;
            }

            // Override engine if specified. This used to be a second copy
            // of the mapping above, and a divergent one: an unknown name was
            // silently ignored here while the translation path rejected it,
            // so speech quietly fell back to the default engine.
            if (parser.isSet(engine)) {
                ttsOptions->setOption("engine", static_cast<int>(selectedEngine));
                hasTTSOverrides = true;
            }

            // Apply CLI overrides if any
            if (hasTTSOverrides) {
                m_session->tts()->applyOptions(*ttsOptions);
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
    if (m_imagePath.isEmpty()) {
        processNextTranslation();
        return;
    }
    recognizeImage();
}

// The source text comes from a picture. Everything downstream is unchanged:
// what recognition produces is the source text, and is then translated,
// spoken and printed exactly as typed text would be.
void Cli::recognizeImage()
{
    // The window does this when it loads its settings; nothing had done it
    // here, so Tesseract had no languages and reported itself unconfigured
    // however well it was actually set up.
    connect(m_session, &TranslationSession::ocrLanguagesUnavailable, this, [this](const QString &languages) {
        printError(tr("Error: Unable to initialize Tesseract with %1").arg(languages));
    });
    m_session->initTesseractFromSettings();

    if (!m_session->prepareOcr()) {
        // prepareOcr() reports through UserNotifier, whose delivery is queued.
        // Quitting from here would return to the event loop with that
        // notification still pending and never printed, so the exit is queued
        // behind it - same thread, so it is delivered first.
        QMetaObject::invokeMethod(
            this,
            [this] {
                cleanup();
                QCoreApplication::exit(1);
            },
            Qt::QueuedConnection);
        return;
    }

    QImage source;
    if (!TranslationSession::loadImage(m_imagePath, source)) {
        printError(tr("Error: Unable to read image '%1'").arg(m_imagePath));
        cleanup();
        QCoreApplication::exit(1);
        return;
    }

    AOcrProvider *engine = m_session->ocr();
    connect(engine, &AOcrProvider::recognized, this, [this](const QString &text) {
        m_sourceText = text;
        if (m_sourceText.isEmpty()) {
            printError(tr("Error: No text was recognized in the image"));
            cleanup();
            QCoreApplication::exit(1);
            return;
        }
        processNextTranslation();
    });
    connect(engine, &AOcrProvider::failed, this, [this](const QString &error) {
        printError(tr("Error: Recognition failed: %1").arg(error));
        cleanup();
        QCoreApplication::exit(1);
    });
    connect(engine, &AOcrProvider::canceled, this, [this] {
        printError(tr("Error: Recognition was cancelled"));
        cleanup();
        QCoreApplication::exit(1);
    });

    // 96 dpi, the same assumption the window makes for an image that arrived
    // as a file rather than off a screen whose scale is known.
    engine->recognize(source, 96);
}

void Cli::onTranslationStateChanged(ATranslationProvider::State state)
{
    qDebug() << "CLI: Translation state changed to:" << static_cast<int>(state) << "Error:" << static_cast<int>(m_session->translator()->error);
    if (state == ATranslationProvider::State::Processed) {
        // Check for translation error
        if (m_session->translator()->error != ATranslationProvider::TranslationError::NoError) {
            const QString errorString = m_session->translator()->getErrorString();
            if (!errorString.isEmpty()) {
                printError(tr("Error: %1").arg(errorString));
            } else {
                printError(tr("Translation error occurred"));
            }
            flushJsonOutput();
            cleanup();
            QCoreApplication::exit(1);
            return;
        }

        // Translation successful
        m_currentTranslationResult = m_session->translator()->result;

        // Update source language with detected language if auto-detection was used
        if (m_sourceLang == Language::autoLanguage() && m_session->translator()->sourceLanguage != Language::autoLanguage()) {
            m_sourceLang = m_session->translator()->sourceLanguage;
            qDebug() << "Auto-detected source language:" << m_sourceLang.name();
        }

        m_session->acceptTranslation();

        if (!m_audioOnly) {
            printTranslation();
        }

        // Handle TTS - only start if no TTS is currently active
        if (m_ttsState == TTSState::None) {
            if (m_speakSource) {
                m_ttsState = TTSState::SpeakingSource;
                m_waitingForTTS = true;
                if (speakText(m_sourceText, m_sourceLang))
                    return;
            } else if (m_speakTranslation) {
                m_ttsState = TTSState::SpeakingTranslation;
                m_waitingForTTS = true;
                if (speakText(m_currentTranslationResult.translation, m_currentTargetLang))
                    return;
            }
            // Speech never started, so nothing will report that it finished.
            // Fall through and advance the run instead of waiting.
            m_waitingForTTS = false;
        }

        // No TTS needed, move to next translation
        m_currentTranslationIndex++;
        m_ttsState = TTSState::None;
        processNextTranslation();
    } else if (state == ATranslationProvider::State::Finished) {
        // Check for translation error
        if (m_session->translator()->error != ATranslationProvider::TranslationError::NoError) {
            const QString errorString = m_session->translator()->getErrorString();
            if (!errorString.isEmpty()) {
                printError(tr("Error: %1").arg(errorString));
            } else {
                printError(tr("Translation error occurred"));
            }
            // std::exit() here skipped every QTextStream destructor, so
            // anything still buffered - including the JSON document - was
            // lost on the way out. Unwind through the event loop instead.
            flushJsonOutput();
            cleanup();
            QCoreApplication::exit(1);
            return;
        }

        // Reset for next translation
        m_session->resetTranslator();
    }
}

void Cli::onTTSStateChanged(QTextToSpeech::State state)
{
    qDebug() << "TTS state changed to:" << state << "Current TTS state:" << static_cast<int>(m_ttsState);
    qDebug() << "m_speakSource:" << m_speakSource << "m_speakTranslation:" << m_speakTranslation;

    if (state == QTextToSpeech::Ready) {
        if (m_ttsState == TTSState::SpeakingSource && m_speakTranslation) {
            qDebug() << "Transitioning from source to translation speech";
            qDebug() << "Translation text:" << m_currentTranslationResult.translation;
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

            if (m_session->tts() == nullptr) {
                qWarning() << "TTS provider is null during transition";
                m_waitingForTTS = false;
                m_ttsState = TTSState::None;
                m_currentTranslationIndex++;
                processNextTranslation();
                return;
            }

            qDebug() << "About to call speakText for translation";
            // The translation only. Reading the transliteration and the
            // dictionary entries aloud after it was never intended; it only
            // happened because everything arrived as one blob of markup.
            if (speakText(m_currentTranslationResult.translation, m_currentTargetLang))
                return;

            m_waitingForTTS = false;
            m_ttsState = TTSState::None;
            m_currentTranslationIndex++;
            processNextTranslation();
            return;
        }

        qDebug() << "TTS finished, resetting state and moving to next translation";
        m_waitingForTTS = false;
        m_ttsState = TTSState::None;

        // Move to next translation
        m_currentTranslationIndex++;
        processNextTranslation();
    } else if (state == QTextToSpeech::Error) {
        const QString errorString = (m_session->tts() != nullptr) ? m_session->tts()->errorString() : tr("Unknown error");
        printError(tr("Error: TTS failed: %1").arg(errorString));
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
    // Through the session, the same door the window uses. targetLang is
    // already concrete by this point - "auto" was resolved once, up in
    // process() - so the resolution inside has nothing left to do here.
    m_session->requestTranslation(text, targetLang, sourceLang);
}

void Cli::processNextTranslation()
{
    if (m_currentTranslationIndex >= m_translationLanguages.size()) {
        flushJsonOutput();
        cleanup();
        QCoreApplication::exit(m_exitCode);
        return;
    }

    const Language targetLang = m_translationLanguages[m_currentTranslationIndex];
    translateText(m_sourceText, m_sourceLang, targetLang);
}

void Cli::printTranslation()
{
    // JSON mode: collect, do not print. One target language produced one
    // top-level object, so asking for several emitted several of them back to
    // back - which is not a JSON document and no ordinary parser will read it.
    if (m_json) {
        QJsonObject entry;
        entry[QStringLiteral("language")] = m_currentTargetLang.toCode();
        entry[QStringLiteral("language_name")] = m_currentTargetLang.name();
        entry[QStringLiteral("text")] = m_currentTranslationResult.translation;
        if (!m_currentTranslationResult.translationTranslit.isEmpty())
            entry[QStringLiteral("transliteration")] = m_currentTranslationResult.translationTranslit;
        if (!m_currentTranslationResult.sourceTranslit.isEmpty())
            entry[QStringLiteral("source_transliteration")] = m_currentTranslationResult.sourceTranslit;
        if (!m_currentTranslationResult.sourceTranscription.isEmpty())
            entry[QStringLiteral("source_transcription")] = m_currentTranslationResult.sourceTranscription;
        m_jsonTranslations.append(entry);
        return;
    }

    // Short mode
    if (m_brief) {
        m_stdout << m_currentTranslationResult.translation << Qt::endl;
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
        m_stdout << resultToPlainText(m_currentTranslationResult) << '\n';
        m_stdout << '\n';
    }

    m_stdout.flush();
}

void Cli::flushJsonOutput()
{
    if (!m_json || m_jsonEmitted)
        return;

    m_jsonEmitted = true;

    QJsonObject root;
    root[QStringLiteral("source")] = m_sourceText;
    // Codes, not display names: "English" is for people, and this output is
    // not for people. The name is kept alongside for the ones that have no
    // familiar code.
    root[QStringLiteral("source_language")] = m_sourceLang.toCode();
    root[QStringLiteral("source_language_name")] = m_sourceLang.name();
    root[QStringLiteral("translations")] = m_jsonTranslations;

    m_stdout << QJsonDocument(root).toJson();
    m_stdout.flush();
}

bool Cli::speakText(const QString &text, const Language &language)
{
    qDebug() << "=== ENTERED speakText ===";
    qDebug() << "Text:" << text;
    qDebug() << "Language:" << language.name();

    if (m_session->tts() == nullptr) {
        reportSpeechUnavailable(tr("no provider was created"));
        return false;
    }

    if (text.isEmpty()) {
        qWarning() << "Cannot speak empty text";
        return false;
    }

    // A provider that cannot speak never emits a state change, and the wait
    // at the end of this function would then never end - the process sat
    // there forever with the translation already printed. Neither way of
    // getting here needs any misconfiguration:
    //
    //   --tts none (or a settings default of None) builds a NoopTTSProvider
    //   whose state() reports Ready and whose say() does nothing whatsoever,
    //   so nothing is ever emitted;
    //
    //   a Qt engine with no reachable speech plugin is already in Error when
    //   it is handed over, so there is no *transition* into Error for
    //   onTTSStateChanged()'s error branch to fire on either.
    //
    // Both have to be caught here, before anything starts waiting.
    if (m_session->tts()->state() == QTextToSpeech::Error) {
        const QString reason = m_session->tts()->errorString();
        reportSpeechUnavailable(reason.isEmpty() ? tr("the engine failed to start") : reason);
        return false;
    }

    if (m_session->tts()->availableLanguages().isEmpty()) {
        reportSpeechUnavailable(tr("the selected provider has no voices available"));
        return false;
    }

    qDebug() << "TTS: Speaking text:" << text << "with language:" << language.name();
    qDebug() << "TTS: Current state before speaking:" << m_session->tts()->state();

    // Find the best available locale for TTS
    const Language bestLanguage = findBestTTSLanguage(language);
    qDebug() << "TTS: Using best locale:" << bestLanguage.name();

    // Set locale and find appropriate voice
    m_session->tts()->setLanguage(bestLanguage);
    QList<Voice> availableVoices = m_session->tts()->findVoices(bestLanguage);
    if (!availableVoices.isEmpty()) {
        m_session->tts()->setVoice(availableVoices.first());
        qDebug() << "TTS: Selected voice:" << availableVoices.first().name() << "model path:" << availableVoices.first().modelPath();
    } else {
        qDebug() << "TTS: No voices found for locale, using current voice";
    }

    if (m_session->tts()->state() == QTextToSpeech::Ready) {
        qDebug() << "TTS: Calling say() directly";
        m_session->tts()->say(text);
        qDebug() << "TTS: say() call completed";
    } else {
        qDebug() << "TTS: Waiting for Ready state before calling say(), current state:" << m_session->tts()->state();
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(m_session->tts(), &ATTSProvider::stateChanged, this, [this, text, connection](QTextToSpeech::State state) {
            if (state == QTextToSpeech::Ready) {
                qDebug() << "TTS: Ready state reached, calling say()";
                disconnect(*connection);
                m_session->tts()->say(text);
                qDebug() << "TTS: say() call completed from wait";
            }
        });
    }
    qDebug() << "TTS: speakText() method completed";
    return true;
}

void Cli::printNotification(const UserNotifier::Notification &notification)
{
    // stderr, like every other diagnostic: stdout carries the translation.
    // Only the summary - the details are written for a dialog, some of them
    // as HTML, and none of them belong in a terminal.
    printError(notification.title.isEmpty() ? notification.text : QStringLiteral("%1: %2").arg(notification.title, notification.text));
}

void Cli::reportSpeechUnavailable(const QString &reason)
{
    // The user asked for speech and is not getting it, so the run has not
    // done what it was told to; say so in the exit code as well.
    m_exitCode = 1;
    if (m_speechUnavailableReported)
        return;

    m_speechUnavailableReported = true;
    printError(tr("Error: text-to-speech is unavailable: %1").arg(reason));
}

void Cli::printLangCodes()
{
    // Was a hardcoded list of 34 QLocale entries that had nothing to do with
    // the selected provider. It left out every language Mozhi registers which
    // QLocale has no equivalent for - Sranan Tongo, Hill Mari, the creoles,
    // some forty of them - so precisely the codes a user cannot guess were the
    // ones --codes would not tell them, while the option called itself
    // "Display all language codes".
    //
    // Ask the provider, which already has to answer this for the GUI's
    // language lists. Source and destination are listed separately only when
    // they actually differ; every backend in tree returns one set for both.
    const QVector<Language> sourceLanguages = m_session->translator()->supportedSourceLanguages();
    const QVector<Language> destinationLanguages = m_session->translator()->supportedDestinationLanguages();

    if (sourceLanguages == destinationLanguages) {
        printLanguageList(sourceLanguages);
    } else {
        m_stdout << tr("Source languages:") << '\n';
        printLanguageList(sourceLanguages);
        m_stdout << '\n'
                 << tr("Translation languages:") << '\n';
        printLanguageList(destinationLanguages);
    }
    m_stdout.flush();
}

void Cli::printLanguageList(const QVector<Language> &languages)
{
    for (const Language &language : languages) {
        const QString code = language.toCode();
        if (code.isEmpty())
            continue;
        m_stdout << code << " - " << language.name() << '\n';
    }
}

// Every caller quits immediately afterwards. What this has to do is make sure
// nothing else arrives in the meantime: the loop does not stop the instant
// quit() is called, and a queued provider signal delivered after the exit code
// has been decided would run a handler for a run that is already over.
//
// It used to deleteLater() both providers, which did not achieve that - a
// deferred delete scheduled just before the loop stops never runs at all. What
// it actually did was null the two pointers, so the handlers' null checks
// short-circuited. Severing the connections says that outright.
void Cli::cleanup()
{
    m_session->disconnect(this);
}

std::optional<OnlineTranslator::Engine> Cli::engineFromName(const QString &name)
{
    static const QHash<QString, OnlineTranslator::Engine> engines = {
        {QStringLiteral("google"), OnlineTranslator::Google},
        {QStringLiteral("yandex"), OnlineTranslator::Yandex},
        {QStringLiteral("deepl"), OnlineTranslator::Deepl},
        {QStringLiteral("bing"), OnlineTranslator::Duckduckgo},
        {QStringLiteral("duckduckgo"), OnlineTranslator::Duckduckgo},
        {QStringLiteral("libretranslate"), OnlineTranslator::LibreTranslate},
        {QStringLiteral("mymemory"), OnlineTranslator::Mymemory},
        {QStringLiteral("reverso"), OnlineTranslator::Reverso},
    };

    const auto found = engines.constFind(name.toLower());
    if (found == engines.constEnd())
        return std::nullopt;
    return *found;
}

void Cli::exitWithUsage(const QCommandLineParser &parser, const QString &message)
{
    printError(message);
    errorStream() << parser.helpText() << Qt::flush;
    ::exit(1);
}

// names().at(1) assumed every option has a short name and a long one. An
// option declared with only a long name - --image is the first - made that an
// out-of-range access, so `crow --image x -f` aborted instead of printing the
// error it was about to print. The long name is simply the last one.
QString Cli::longOptionName(const QCommandLineOption &option)
{
    return option.names().constLast();
}

void Cli::checkIncompatibleOptions(QCommandLineParser &parser, const QCommandLineOption &option1, const QCommandLineOption &option2)
{
    if (parser.isSet(option1) && parser.isSet(option2)) {
        exitWithUsage(parser, tr("Error: You can't use --%1 with --%2").arg(longOptionName(option1), longOptionName(option2)));
    }
}

QByteArray Cli::readFilesFromStdin()
{
    const QString stdinText = QTextStream(stdin).readAll();
    QByteArray filesData;
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    for (const QString &filePath : stdinText.split(whitespace, Qt::SkipEmptyParts)) {
        QFile file(filePath);
        if (!file.exists()) {
            printError(tr("Error: File does not exist: %1").arg(file.fileName()));
            continue;
        }

        if (!file.open(QFile::ReadOnly)) {
            printError(tr("Error: Unable to open file: %1").arg(file.fileName()));
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
            printError(tr("Error: File does not exist: %1").arg(file.fileName()));
            continue;
        }

        if (!file.open(QFile::ReadOnly)) {
            printError(tr("Error: Unable to open file: %1").arg(file.fileName()));
            continue;
        }

        filesData += file.readAll();
    }

    return filesData;
}

Language Cli::findBestTTSLanguage(const Language &requestedLanguage)
{
    qDebug() << "findBestTTSLanguage called with:" << requestedLanguage.name();

    if (m_session->tts() == nullptr) {
        qDebug() << "findBestTTSLocale: TTS provider is null";
        return Language(QLocale::system());
    }

    qDebug() << "findBestTTSLocale: Getting available locales...";
    // Get all available locales from TTS
    QList<Language> availableLanguages = m_session->tts()->availableLanguages();
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
