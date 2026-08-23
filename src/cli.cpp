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
#include "translator/translationlogic.h"
#include "tts/attsprovider.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QHash>
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

// Every backend builds its result as HTML, because MainWindow renders it with
// setHtml(). A terminal is not a QTextEdit, so the CLI was printing that
// markup literally - "hallo Welt<br><font color=\"grey\"><i>/.../</i></font>"
// - in plain, --brief and --json output alike, and reading the tags out loud
// when asked to speak the translation.
//
// Undo the rendering. The vocabulary is small and entirely ours: <br> for
// line breaks, <b>/<i>/<font> for emphasis, &nbsp; for the indent on example
// lines. Tags go before entities are decoded, so source text that genuinely
// contained "<b>" - which the backend escaped to "&lt;b&gt;" - survives
// instead of being mistaken for markup and dropped.
//
// This is a stopgap. The real fix is for backends to hand over the fields
// (translation, transliterations, examples) and let each frontend render
// them, rather than shipping one frontend's rendering to all of them.
QString htmlResultToPlainText(const QString &html)
{
    static const QRegularExpression lineBreak(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression anyTag(QStringLiteral("<[^>]*>"));

    QString text = html;
    text.replace(lineBreak, QStringLiteral("\n"));
    text.remove(anyTag);

    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    // Last of the entities: decoding "&amp;" any earlier would turn an
    // escaped "&amp;lt;" - a literal "&lt;" in the text - back into a "<".
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return text;
}
} // namespace

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
        exitWithUsage(parser,
                      tr("Error: For --%1 you must specify --%2 and/or --%3 options")
                          .arg(audioOnly.names().at(1), speakSource.names().at(1), speakTranslation.names().at(1)));
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

    m_translator = ATranslationProvider::createTranslationProvider(this, translationBackend);
    connect(m_translator, &ATranslationProvider::stateChanged, this, &Cli::onTranslationStateChanged);

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
        // The rule the window already uses, via the same function
        // (MainWindow::preferredTranslationLanguage calls it too), so "auto"
        // does not mean two different things depending on which frontend
        // asked. The CLI used to take the system locale unconditionally and
        // ignore the configured primary/secondary pair, so `crow -s en text`
        // on an English system asked for English to English and got the
        // source back - while the window, given the same settings and the
        // same text, translated it into the primary language.
        m_translationLanguages.append(TranslationLogic::preferredDestination(m_sourceLang,
                                                                             settings.primaryLanguage(),
                                                                             settings.secondaryLanguage(),
                                                                             Language(QLocale::system())));
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

    if (m_sourceText.isEmpty()) {
        exitWithUsage(parser, tr("Error: There is no text for translation"));
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
            options->setOption("engine", static_cast<int>(selectedEngine));
            hasOverrides = true;
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
        // Converted once, here: printTranslation() has three output modes
        // and speakText() must not read markup aloud either.
        m_currentTranslationResult = htmlResultToPlainText(m_translator->result);

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
                if (speakText(m_sourceText, m_sourceLang))
                    return;
            } else if (m_speakTranslation) {
                m_ttsState = TTSState::SpeakingTranslation;
                m_waitingForTTS = true;
                if (speakText(m_currentTranslationResult, m_currentTargetLang))
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
        if (m_translator->error != ATranslationProvider::TranslationError::NoError) {
            const QString errorString = m_translator->getErrorString();
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
            if (speakText(m_currentTranslationResult, m_currentTargetLang))
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
        const QString errorString = (m_tts != nullptr) ? m_tts->errorString() : tr("Unknown error");
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
    m_translator->translate(text, targetLang, sourceLang);
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
        entry[QStringLiteral("text")] = m_currentTranslationResult;
        m_jsonTranslations.append(entry);
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

    if (m_tts == nullptr) {
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
    if (m_tts->state() == QTextToSpeech::Error) {
        const QString reason = m_tts->errorString();
        reportSpeechUnavailable(reason.isEmpty() ? tr("the engine failed to start") : reason);
        return false;
    }

    if (m_tts->availableLanguages().isEmpty()) {
        reportSpeechUnavailable(tr("the selected provider has no voices available"));
        return false;
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
    return true;
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
    const QVector<Language> sourceLanguages = m_translator->supportedSourceLanguages();
    const QVector<Language> destinationLanguages = m_translator->supportedDestinationLanguages();

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

void Cli::checkIncompatibleOptions(QCommandLineParser &parser, const QCommandLineOption &option1, const QCommandLineOption &option2)
{
    if (parser.isSet(option1) && parser.isSet(option2)) {
        exitWithUsage(parser, tr("Error: You can't use --%1 with --%2").arg(option1.names().at(1), option2.names().at(1)));
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
