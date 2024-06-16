/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CLI_H
#define CLI_H

#include "onlinetranslator/onlinetranslator.h"

#include <QObject>
#include <QTextStream>
#include <QVector>

class QCoreApplication;
class QMediaPlayer;
class QStateMachine;
class QCommandLineParser;
class QCommandLineOption;

class Cli : public QObject
{
    Q_OBJECT

public:
    explicit Cli(QObject *parent = nullptr);

    void process(const QCoreApplication &app);

private slots:
    void requestTranslation();
    void parseTranslation();
    void printTranslation();

    void requestLanguage();
    void parseLanguage();

    void speakSource();
    void speakTranslation();

    void printLangCodes();

private:
    // Main state machines
    void buildShowCodesStateMachine();
    void buildTranslationStateMachine();

    // Helpers
    void speak(const QString &text, OnlineTranslator::Language lang);
    static void checkIncompatibleOptions(QCommandLineParser &parser, const QCommandLineOption &option1, const QCommandLineOption &option2);

    static QByteArray readFilesFromStdin();
    static QByteArray readFilesFromArguments(const QStringList &arguments);

    static constexpr char s_langProperty[] = "Language";

    QMediaPlayer *m_player;
    OnlineTranslator *m_translator;
    QStateMachine *m_stateMachine;
    QTextStream m_stdout{stdout};

    QString m_sourceText;
    QVector<OnlineTranslator::Language> m_translationLanguages;
    OnlineTranslator::Engine m_engine = OnlineTranslator::Google;
    OnlineTranslator::Language m_sourceLang = OnlineTranslator::NoLanguage;
    OnlineTranslator::Language m_uiLang = OnlineTranslator::NoLanguage;
    bool m_speakSource = false;
    bool m_speakTranslation = false;
    bool m_sourcePrinted = false;
    bool m_brief = false;
    bool m_audioOnly = false;
    bool m_json = false;
};

#endif // CLI_H
