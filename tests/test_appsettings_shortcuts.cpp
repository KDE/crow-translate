/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The global shortcuts as a fresh installation has them. Each one reads its own
// key with its own default; a getter that falls back to a neighbour's default
// puts two actions on one key, and QHotkey then fires both of them on it.

#include "testisolation.h"
#include "settings/appsettings.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QTest>

class AppSettingsShortcutsTest : public QObject
{
    Q_OBJECT

private slots:
    void testEachGlobalShortcutStartsAtItsOwnDefault()
    {
        const AppSettings settings;
        QCOMPARE(settings.translateSelectionShortcut(), AppSettings::defaultTranslateSelectionShortcut());
        QCOMPARE(settings.speakSelectionShortcut(), AppSettings::defaultSpeakSelectionShortcut());
        QCOMPARE(settings.speakTranslatedSelectionShortcut(), AppSettings::defaultSpeakTranslatedSelectionShortcut());
        QCOMPARE(settings.stopSpeakingShortcut(), AppSettings::defaultStopSpeakingShortcut());
        // Fell back to stop's default, Ctrl+Alt+G, so play/pause and stop
        // shared a key and Ctrl+Alt+D did nothing.
        QCOMPARE(settings.playPauseSpeakingShortcut(), AppSettings::defaultPlayPauseSpeakingShortcut());
        QCOMPARE(settings.showMainWindowShortcut(), AppSettings::defaultShowMainWindowShortcut());
        QCOMPARE(settings.copyTranslatedSelectionShortcut(), AppSettings::defaultCopyTranslatedSelectionShortcut());
        QCOMPARE(settings.recognizeScreenAreaShortcut(), AppSettings::defaultRecognizeScreenAreaShortcut());
        QCOMPARE(settings.translateScreenAreaShortcut(), AppSettings::defaultTranslateScreenAreaShortcut());
        QCOMPARE(settings.delayedRecognizeScreenAreaShortcut(), AppSettings::defaultDelayedRecognizeScreenAreaShortcut());
        QCOMPARE(settings.delayedTranslateScreenAreaShortcut(), AppSettings::defaultDelayedTranslateScreenAreaShortcut());
    }

    void testNoTwoGlobalShortcutsShareADefaultKey()
    {
        const AppSettings settings;
        const QList<QKeySequence> keys = {
            settings.translateSelectionShortcut(),
            settings.speakSelectionShortcut(),
            settings.speakTranslatedSelectionShortcut(),
            settings.stopSpeakingShortcut(),
            settings.playPauseSpeakingShortcut(),
            settings.showMainWindowShortcut(),
            settings.copyTranslatedSelectionShortcut(),
            settings.recognizeScreenAreaShortcut(),
            settings.translateScreenAreaShortcut(),
            settings.delayedRecognizeScreenAreaShortcut(),
            settings.delayedTranslateScreenAreaShortcut(),
        };
        for (qsizetype i = 0; i < keys.size(); ++i) {
            for (qsizetype j = i + 1; j < keys.size(); ++j) {
                if (!keys.at(i).isEmpty())
                    QVERIFY2(keys.at(i) != keys.at(j), qPrintable(keys.at(i).toString() + QStringLiteral(" twice")));
            }
        }
    }
};

int main(int argc, char *argv[])
{
    isolateTestSettings();
    QCoreApplication app(argc, argv);
    AppSettingsShortcutsTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_appsettings_shortcuts.moc"
