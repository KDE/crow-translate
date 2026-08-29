/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Boundary guard, in the style of test_dbus_static_init: the translation, TTS
// and OCR-engine layers must not depend on QtWidgets.
//
// They used to. Every one of those layers put its own QMessageBox on screen,
// which meant a backend decided what a message looked like, a CLI run built
// dialogs nobody would ever see, and any frontend without widgets - or any
// backend that becomes a loadable plugin - had to link a widget toolkit to do
// nothing with it. That is fixed; this keeps it fixed, because the failure
// mode is silent. Adding "#include <QMessageBox>" back compiles perfectly
// well, since the application links QtWidgets anyway for the GUI.
//
// Pure source-text scan. No display, no QApplication.

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTest>

class WidgetFreeCoreTest : public QObject
{
    Q_OBJECT

private slots:
    void testCoreLayersDoNotIncludeQtWidgets()
    {
        // The layers that have to run without a GUI: they are what the CLI
        // drives today and what becomes loadable plugins under work item #738.
        const QStringList scannedDirs = {
            QStringLiteral("core"),
            QStringLiteral("translator"),
            QStringLiteral("tts"),
            QStringLiteral("ocr"),
            QStringLiteral("llm"),
        };

        // Genuine display-server integration, and deliberately exempt:
        //
        //   snippingarea is a full-screen selection overlay - it *is* a
        //   widget, and there is nothing to abstract about that;
        //
        //   the screen grabbers talk to X11, KWin, GNOME Shell and the
        //   xdg-desktop-portal, and the portal one needs its parent's
        //   windowHandle() to pass a parent-window token.
        //
        // Both belong to whatever frontend owns a screen, and both move out
        // of here when the frontend is separated (#737).
        const QStringList exemptPathFragments = {
            QStringLiteral("/ocr/snippingarea."),
            QStringLiteral("/ocr/screengrabbers/"),
        };

        static const QRegularExpression widgetInclude(
            QStringLiteral("^\\s*#\\s*include\\s*<Q(MessageBox|Widget|Application|Dialog|ComboBox|PushButton|Label|MainWindow|FileDialog|CheckBox|LineEdit|"
                           "PlainTextEdit|TextEdit|Menu|ToolButton|ListWidget|TableWidget|GroupBox|SpinBox|Layout|BoxLayout|GridLayout|FormLayout|"
                           "StatusBar|ToolBar|SystemTrayIcon|Style|Frame|ScrollArea|TabWidget|Splitter|Wizard|ColorDialog|FontDialog|InputDialog|"
                           "ProgressDialog|ErrorMessage)>"),
            QRegularExpression::MultilineOption);

        QStringList offenders;

        for (const QString &dir : scannedDirs) {
            const QString root = QStringLiteral(SRC_DIR) + QLatin1Char('/') + dir;
            if (!QDir(root).exists())
                continue;

            QDirIterator it(root, {QStringLiteral("*.h"), QStringLiteral("*.cpp")}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString filePath = it.next();
                if (filePath.contains(QStringLiteral("/3rdparty/")))
                    continue;

                bool exempt = false;
                for (const QString &fragment : exemptPathFragments) {
                    if (filePath.contains(fragment)) {
                        exempt = true;
                        break;
                    }
                }
                if (exempt)
                    continue;

                QFile file(filePath);
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                    continue;

                const QString contents = QString::fromUtf8(file.readAll());
                QRegularExpressionMatchIterator matches = widgetInclude.globalMatch(contents);
                while (matches.hasNext())
                    offenders << QStringLiteral("%1: %2").arg(filePath, matches.next().captured().trimmed());
            }
        }

        QVERIFY2(offenders.isEmpty(),
                 qPrintable(QStringLiteral("QtWidgets is back in the headless layers:\n  %1\n\n"
                                           "Report it through UserNotifier and let the frontend decide what it looks like.")
                                .arg(offenders.join(QStringLiteral("\n  ")))));
    }
};

QTEST_GUILESS_MAIN(WidgetFreeCoreTest)

#include "test_widget_free_core.moc"
