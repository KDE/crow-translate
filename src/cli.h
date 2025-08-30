/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CLI_H
#define CLI_H

#include "language.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QLocale>
#include <QObject>
#include <QTextStream>
#include <QVector>

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
    void speakText(const QString &text, const Language &language);
    void printLangCodes();
    void cleanup();

    // Helper method to find best available TTS locale
    Language findBestTTSLanguage(const Language &requestedLanguage);

    // Helpers
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
    bool m_waitingForTTS = false;

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
