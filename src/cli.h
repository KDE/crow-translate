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
#include "core/translationresult.h"
#include "core/usernotifier.h"
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
class TranslationSession;

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
    // Runs OCR over --image and hands what it finds to the normal flow as the
    // source text.
    void recognizeImage();
    void processNextTranslation();
    void printTranslation();
    // Emitted once, at the end of the run: --json produces a document, and a
    // document cannot be written a piece at a time.
    void flushJsonOutput();
    // False means speech could not be started at all, so the caller must
    // advance the run itself rather than wait for a state change.
    bool speakText(const QString &text, const Language &language);
    void reportSpeechUnavailable(const QString &reason);
    void printNotification(const UserNotifier::Notification &notification);
    void printLangCodes();
    void printLanguageList(const QVector<Language> &languages);
    // Releases the providers early, before the event loop stops. Only the
    // paths that quit while a provider may still be mid-request need it.
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
    // The long form of an option's name, for building messages about it.
    static QString longOptionName(const QCommandLineOption &option);
    static void checkIncompatibleOptions(QCommandLineParser &parser, const QCommandLineOption &option1, const QCommandLineOption &option2);
    static QByteArray readFilesFromStdin();
    static QByteArray readFilesFromArguments(const QStringList &arguments);

    // The same core the window drives. Both providers, the options manager
    // and the language rule live in here, so "auto" and a backend choice
    // cannot mean one thing on the command line and another in the window -
    // which is exactly what they used to do.
    TranslationSession *m_session = nullptr;
    QTextStream m_stdout{stdout};

    QString m_sourceText;
    QString m_imagePath;
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

    TranslationResult m_currentTranslationResult;
    Language m_currentTargetLang;

    enum class TTSState {
        None,
        SpeakingSource,
        SpeakingTranslation
    };
    TTSState m_ttsState = TTSState::None;
};

#endif // CLI_H
