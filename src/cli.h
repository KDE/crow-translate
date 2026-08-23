/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CLI_H
#define CLI_H

#include "language.h"
#include "onlinetranslator.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QJsonArray>
#include <QLocale>
#include <QObject>
#include <QTextStream>
#include <QVector>

#include <optional>

class QCoreApplication;
class QCommandLineParser;
class QCommandLineOption;

class Cli : public QObject
{
    Q_OBJECT

public:
    explicit Cli(QObject *parent = nullptr);

    void process(const QCoreApplication &app);

private slots:
    void onTranslationStateChanged(ATranslationProvider::State state);
    void onTTSStateChanged(QTextToSpeech::State state);

private:
    void translateText(const QString &text, const Language &sourceLang, const Language &targetLang);
    void processNextTranslation();
    void printTranslation();
    // Emitted once, at the end of the run: --json produces a document, and a
    // document cannot be written a piece at a time.
    void flushJsonOutput();
    // False means speech could not be started at all, so the caller must
    // advance the run itself rather than wait for a state change.
    bool speakText(const QString &text, const Language &language);
    void reportSpeechUnavailable(const QString &reason);
    void printLangCodes();
    void printLanguageList(const QVector<Language> &languages);
    void cleanup();

    // Helper method to find best available TTS locale
    Language findBestTTSLanguage(const Language &requestedLanguage);

    // Helpers
    // The one place that decides what an engine name means. Returns nullopt
    // for a name no engine answers to; "lingva" is deliberately absent -
    // Mozhi dropped it, and it was advertised in --help long after.
    static std::optional<OnlineTranslator::Engine> engineFromName(const QString &name);
    // Prints the reason, then the usage text, then exits 1 - all on stderr.
    // QCommandLineParser::showHelp() cannot be used for this: it writes to
    // stdout regardless of the exit code it is given, which drops forty lines
    // of usage into the stream a caller is reading translations out of.
    [[noreturn]] static void exitWithUsage(const QCommandLineParser &parser, const QString &message);
    static void checkIncompatibleOptions(QCommandLineParser &parser, const QCommandLineOption &option1, const QCommandLineOption &option2);
    static QByteArray readFilesFromStdin();
    static QByteArray readFilesFromArguments(const QStringList &arguments);

    ATranslationProvider *m_translator = nullptr;
    ATTSProvider *m_tts = nullptr;
    QTextStream m_stdout{stdout};

    QString m_sourceText;
    QVector<Language> m_translationLanguages;
    Language m_sourceLang;
    int m_currentTranslationIndex = 0;

    bool m_speakSource = false;
    bool m_speakTranslation = false;
    bool m_sourcePrinted = false;
    bool m_brief = false;
    bool m_audioOnly = false;
    bool m_json = false;
    QJsonArray m_jsonTranslations;
    bool m_jsonEmitted = false;
    bool m_waitingForTTS = false;
    // Reported once, however many translations are in the run.
    bool m_speechUnavailableReported = false;
    int m_exitCode = 0;

    QString m_currentTranslationResult;
    Language m_currentTargetLang;

    enum class TTSState {
        None,
        SpeakingSource,
        SpeakingTranslation
    };
    TTSState m_ttsState = TTSState::None;
};

#endif // CLI_H
