/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// "Force source language detection" and "Force translation language detection"
// make a global action switch the language buttons to Auto. That was written
// out in each action - translate the selection, speak it, every way a
// recognition starts - each reading the two settings for itself, and the
// copies drifted: none of them asked whether the backend can detect a language
// at all, which the one function they had all been copied from did.
//
// There is one owner now: TranslationLogic::forcedAuto() is the rule,
// TranslationSession::forcedAutoFor() reads the settings for it, and
// MainWindow::applyForcedAuto() is the only place the buttons are switched.
// This holds that in place by reading the sources, as test_dbus_static_init
// and test_widget_free_core do for their rules: the next action that needs
// forcing calls applyForcedAuto(), it does not get a copy of its own.

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTest>

class ForcedAutoOwnerTest : public QObject
{
    Q_OBJECT

private:
    static QStringList linesMatching(const QString &relativePath, const QRegularExpression &pattern)
    {
        QStringList found;
        QFile file(QStringLiteral(SRC_DIR "/") + relativePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {QStringLiteral("cannot read %1").arg(relativePath)};
        }
        int number = 0;
        while (!file.atEnd()) {
            ++number;
            const QString line = QString::fromUtf8(file.readLine());
            if (pattern.match(line).hasMatch()) {
                found << QStringLiteral("%1:%2: %3").arg(relativePath).arg(number).arg(line.trimmed());
            }
        }
        return found;
    }

private slots:
    // The settings are read where they are stored, where they are edited, and
    // by the session. Nowhere else.
    void testTheTwoSettingsHaveOneReader()
    {
        static const QRegularExpression reads(QStringLiteral("isForce(Source|Translation)Autodetect\\s*\\("));
        const QStringList allowed = {QStringLiteral("settings/appsettings.h"),
                                     QStringLiteral("settings/appsettings.cpp"),
                                     QStringLiteral("settings/settingsdialog.cpp"),
                                     QStringLiteral("core/translationsession.cpp")};

        QStringList offenders;
        const QDir root(QStringLiteral(SRC_DIR));
        QDirIterator it(root.path(), {QStringLiteral("*.h"), QStringLiteral("*.cpp")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString relative = root.relativeFilePath(it.next());
            if (relative.startsWith(QStringLiteral("3rdparty/")) || allowed.contains(relative)) {
                continue;
            }
            offenders << linesMatching(relative, reads);
        }
        QVERIFY2(offenders.isEmpty(),
                 qUtf8Printable(QStringLiteral("The force-detection settings are read outside TranslationSession::forcedAutoFor():\n  ")
                                + offenders.join(QStringLiteral("\n  "))));
    }

    // The window switches a side to Auto in applyForcedAuto() - once for the
    // source, once for the destination - and in one more place that is not
    // forcing: when the chosen source is a language the backend does not have.
    void testTheWindowSwitchesToAutoInOnePlace()
    {
        static const QRegularExpression switches(QStringLiteral("LanguagesWidget->checkAutoButton\\s*\\("));
        const QStringList found = linesMatching(QStringLiteral("mainwindow.cpp"), switches);
        QVERIFY2(found.size() == 3,
                 qUtf8Printable(QStringLiteral("Expected the two switches in MainWindow::applyForcedAuto() and the one for an unsupported "
                                               "source language, found %1. A global action that forces Auto calls applyForcedAuto():\n  ")
                                    .arg(found.size())
                                + found.join(QStringLiteral("\n  "))));
    }
};

QTEST_APPLESS_MAIN(ForcedAutoOwnerTest)

#include "test_forcedauto_owner.moc"
