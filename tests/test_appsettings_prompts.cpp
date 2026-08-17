/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// localAiPrompt() has to distinguish "the user wrote this prompt" from "an
// older build's settings dialog auto-saved its default here", because only the
// former may override the current default. Getting that wrong is what made
// ollama keep showing the superseded source-language prompt while a model added
// later showed the new one.

#include "testisolation.h"
#include "settings/appsettings.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTest>

namespace
{

// Byte-for-byte the default that master still ships, and that the old dialog
// persisted under every model whose page was opened.
QString supersededPrompt()
{
    return QStringLiteral(
        "You are a professional {source_lang} ({source_code}) to {target_lang} ({target_code}) translator. "
        "Your goal is to accurately convey the meaning and nuances of the original {source_lang} text "
        "while adhering to {target_lang} grammar, vocabulary, and cultural sensitivities.\n"
        "Produce only the {target_lang} translation, without any additional explanations or commentary. "
        "Please translate the following {source_lang} text into {target_lang}:\n\n\n{text}");
}

void storePrompt(const QString &model, const QString &prompt)
{
    QSettings settings;
    settings.setValue(QStringLiteral("LocalAI/Prompts/") + model, prompt);
    settings.sync();
}

} // namespace

class AppSettingsPromptsTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QSettings settings;
        settings.remove(QStringLiteral("LocalAI"));
        settings.sync();
    }

    void testUnsetPromptIsTheCurrentDefault()
    {
        const QString prompt = AppSettings().localAiPrompt(QStringLiteral("never-configured"));
        QCOMPARE(prompt, AppSettings::defaultLocalAiPrompt());
    }

    // The reported bug, at the level it actually lives: a stored copy of the
    // superseded default must not win over the current default.
    void testSupersededDefaultDoesNotOverrideCurrentDefault()
    {
        storePrompt(QStringLiteral("translategemma_12b_128k:latest"), supersededPrompt());

        const QString prompt = AppSettings().localAiPrompt(QStringLiteral("translategemma_12b_128k:latest"));
        QCOMPARE(prompt, AppSettings::defaultLocalAiPrompt());
        QVERIFY(!prompt.contains(QStringLiteral("{source_lang}")));
    }

    void testEmptyStoredPromptFallsBackToDefault()
    {
        storePrompt(QStringLiteral("half-typed"), QString());

        QCOMPARE(AppSettings().localAiPrompt(QStringLiteral("half-typed")), AppSettings::defaultLocalAiPrompt());
    }

    void testCustomPromptIsReturnedUntouched()
    {
        const QString mine = QStringLiteral("Translate into {target_lang}. Be terse. {text}");
        storePrompt(QStringLiteral("glm-4.5"), mine);

        QCOMPARE(AppSettings().localAiPrompt(QStringLiteral("glm-4.5")), mine);
    }

    // A prompt that merely resembles the superseded default - the user's own
    // edit of it - is still the user's, and must survive.
    void testEditedSupersededPromptIsRespected()
    {
        const QString edited = supersededPrompt() + QStringLiteral("\nKeep proper nouns untranslated.");
        storePrompt(QStringLiteral("my-model"), edited);

        QCOMPARE(AppSettings().localAiPrompt(QStringLiteral("my-model")), edited);
    }

    // A '/' in a model name would otherwise open a nested QSettings group,
    // filing the prompt under a key the reader never looks at.
    void testSlashInModelNameRoundTrips()
    {
        AppSettings settings;
        const QString mine = QStringLiteral("Transcribe, do not translate. {text}");
        settings.setLocalAiPrompt(QStringLiteral("openai/gpt-oss"), mine);

        QCOMPARE(settings.localAiPrompt(QStringLiteral("openai/gpt-oss")), mine);
    }
};

int main(int argc, char *argv[])
{
    isolateTestSettings();
    QCoreApplication app(argc, argv);
    AppSettingsPromptsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_appsettings_prompts.moc"
