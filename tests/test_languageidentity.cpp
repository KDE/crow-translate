/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "language.h"
#include "mainwindow.h"
#include "singleapplication.h"
#include "testisolation.h"
#include "settings/appsettings.h"
#include "settings/settingsdialog.h"

#include <QComboBox>
#include <QSet>
#include <QTest>

class LanguageIdentityTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        isolateTestSettings();
    }

    void aRegionalLanguageKeepsItsIdentity()
    {
        const Language english(QLocale(QStringLiteral("en_001")));
        const Language plainEnglish{QLocale(QLocale::English)};
        QCOMPARE(english.toCode(), QStringLiteral("en"));
        QCOMPARE(english.toCode(), plainEnglish.toCode());
        QVERIFY(english.toQLocale().territory() != QLocale::AnyTerritory);
        QCOMPARE(QLocale::territoryToCode(english.toQLocale().territory()), QStringLiteral("001"));
        QCOMPARE(english.storageCode(), QStringLiteral("en-001"));
        QCOMPARE(plainEnglish.storageCode(), QStringLiteral("en-US"));
        QCOMPARE(Language(english.storageCode()), english);
        QVERIFY(english.displayName() != plainEnglish.displayName());

        QSet<QString> labels;
        for (const Language &language : Language::allLanguages()) {
            const QString label = language.displayName();
            QVERIFY2(!labels.contains(label), qPrintable(QStringLiteral("Duplicate label: %1 (%2)").arg(label, language.toQLocale().name())));
            const Language restored(language.storageCode());
            QCOMPARE(restored.toQLocale().language(), language.toQLocale().language());
            QCOMPARE(restored.toQLocale().territory(), language.toQLocale().territory());
            if (language.toQLocale().script() == QLocale::SimplifiedHanScript || language.toQLocale().script() == QLocale::TraditionalHanScript)
                QCOMPARE(restored.toQLocale().script(), language.toQLocale().script());
            labels.insert(label);
        }
    }

    void regionalSettingsRoundTripAndShortCodesKeepWorking()
    {
        AppSettings settings;
        const Language english(QLocale(QStringLiteral("en_001")));
        settings.setSecondaryLanguage(english);
        QCOMPARE(settings.secondaryLanguage(), english);
        QCOMPARE(AppSettings().secondaryLanguage(), english);
        settings.setSecondaryLanguage(Language(QStringLiteral("de")));
        QCOMPARE(AppSettings().secondaryLanguage().toCode(), QStringLiteral("de"));
    }
    void regionalChoicesSurviveTheSettingsDialog()
    {
        MainWindow window;
        SettingsDialog dialog(&window);
        auto *primary = dialog.findChild<QComboBox *>(QStringLiteral("primaryLangComboBox"));
        auto *secondary = dialog.findChild<QComboBox *>(QStringLiteral("secondaryLangComboBox"));
        QVERIFY(primary != nullptr && secondary != nullptr);
        const int primaryIndex = primary->findText(QStringLiteral("English (en-GB)"));
        const int secondaryIndex = secondary->findText(QStringLiteral("English (en-001)"));
        QVERIFY(primaryIndex >= 0 && secondaryIndex >= 0);
        primary->setCurrentIndex(primaryIndex);
        secondary->setCurrentIndex(secondaryIndex);
        dialog.accept();
        QCOMPARE(AppSettings().primaryLanguage(), Language(QLocale(QStringLiteral("en_GB"))));
        QCOMPARE(AppSettings().secondaryLanguage(), Language(QLocale(QStringLiteral("en_001"))));
        SettingsDialog reopened(&window);
        QCOMPARE(reopened.findChild<QComboBox *>(QStringLiteral("primaryLangComboBox"))->currentIndex(), primaryIndex);
        QCOMPARE(reopened.findChild<QComboBox *>(QStringLiteral("secondaryLangComboBox"))->currentIndex(), secondaryIndex);
    }
};

int main(int argc, char *argv[])
{
    isolateTestSettings();
    SingleApplication app(argc, argv, true);
    LanguageIdentityTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_languageidentity.moc"
