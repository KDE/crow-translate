/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The settings page a backend gets without this dialog knowing anything about
// it. Nothing here is ever shown - the widget is built, inspected by object
// name and thrown away - so it runs without a display like test_statusstrip.
//
// The interesting assertion is the negative one: every backend shipped in
// this repository declares its options as drawn by hand, because the dialog
// has better controls for them than a schema can produce. If that ever stops
// being true, two ways to set the same value appear on the same page, and
// this is what notices.

#include "testisolation.h"
#include "core/pluginmanager.h"
#include "core/settingsstore.h"
#include "settings/appsettings.h"
#include "settings/pluginoptionswidget.h"

#include <QApplication>
#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

#include <memory>

class PluginOptionsWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    // Piper's voices directory is the one option in this project with no
    // hand-built control: it used to be sixty lines of widget construction
    // inside SettingsDialog, and it is a declared option now.
    void testADeclaredDirectoryGetsAControl()
    {
        if (!PluginManager::instance()->contains(PluginFamily::Tts, QStringLiteral("piper")))
            QSKIP("The Piper backend is not installed in this build");

        PluginOptionsWidget widget;
        widget.setBackend(PluginFamily::Tts, QStringLiteral("piper"));

        // Asserted rather than skipped over: if the backend is installed and
        // this control is missing, the option stopped being declared and
        // nothing draws it any more.
        QVERIFY(widget.findChild<QLineEdit *>(QStringLiteral("voices_path")) != nullptr);
        QVERIFY(!widget.isHidden());
    }

    // A backend with nothing to declare draws nothing and takes up no room.
    void testABackendWithNoOptionsIsHidden()
    {
        PluginOptionsWidget widget;
        widget.setBackend(PluginFamily::Tts, QStringLiteral("none"));

        QVERIFY(widget.isHidden());
        QVERIFY(widget.findChild<QLineEdit *>() == nullptr);
    }

    // Everything else shipped here says "I am drawn by hand", so the generic
    // page stays empty beside the real controls rather than duplicating them.
    void testShippedBackendsDeclareTheirOptionsHandDrawn()
    {
        for (const QString &id : {QStringLiteral("mozhi"), QStringLiteral("localai"), QStringLiteral("copy")}) {
            PluginOptionsWidget widget;
            widget.setBackend(PluginFamily::Translation, id);
            QVERIFY2(widget.isHidden(), qPrintable(QStringLiteral("translation backend '%1' would draw a second settings page").arg(id)));
        }
        for (const QString &id : {QStringLiteral("mozhi"), QStringLiteral("qt")}) {
            PluginOptionsWidget widget;
            widget.setBackend(PluginFamily::Tts, id);
            QVERIFY2(widget.isHidden(), qPrintable(QStringLiteral("TTS backend '%1' would draw a second settings page").arg(id)));
        }
        for (const QString &id : {QStringLiteral("tesseract"), QStringLiteral("llm")}) {
            PluginOptionsWidget widget;
            widget.setBackend(PluginFamily::Ocr, id);
            QVERIFY2(widget.isHidden(), qPrintable(QStringLiteral("OCR backend '%1' would draw a second settings page").arg(id)));
        }
    }

    // The value goes to the settings key the backend named, which is the key
    // it has always used - not to some new one derived from the plugin id.
    void testTheValueIsWrittenToTheDeclaredKey()
    {
        PluginOptionsWidget widget;
        widget.setBackend(PluginFamily::Tts, QStringLiteral("piper"));

        auto *edit = widget.findChild<QLineEdit *>(QStringLiteral("voices_path"));
        if (edit == nullptr)
            QSKIP("The Piper backend is not installed in this build");

        edit->setText(QStringLiteral("/tmp/voices-from-the-dialog"));
        widget.save();

        const std::unique_ptr<QSettings> settings(CrowSettings::open());
        QCOMPARE(settings->value(QStringLiteral("TTS/PiperVoicesPath")).toByteArray(),
                 QByteArrayLiteral("/tmp/voices-from-the-dialog"));

        // And back out again, which is what happens when the dialog reopens.
        PluginOptionsWidget reopened;
        reopened.setBackend(PluginFamily::Tts, QStringLiteral("piper"));
        auto *reopenedEdit = reopened.findChild<QLineEdit *>(QStringLiteral("voices_path"));
        QVERIFY(reopenedEdit != nullptr);
        QCOMPARE(reopenedEdit->text(), QStringLiteral("/tmp/voices-from-the-dialog"));
    }

    // A typed value reaches a live provider when editing is finished, and not
    // per keystroke. Reported per keystroke, a voices path was rescanned for
    // every prefix of what was being typed - C:\ alone took 13 s on the GUI
    // thread - with a "no voice models" box for each prefix that held none.
    void testEditingReportsTheChangeWhenItIsFinished()
    {
        PluginOptionsWidget widget;
        widget.setBackend(PluginFamily::Tts, QStringLiteral("piper"));

        auto *edit = widget.findChild<QLineEdit *>(QStringLiteral("voices_path"));
        if (edit == nullptr)
            QSKIP("The Piper backend is not installed in this build");

        QSignalSpy changed(&widget, &PluginOptionsWidget::optionChanged);
        edit->selectAll();
        for (const QChar c : QStringLiteral("/tmp/elsewhere"))
            edit->insert(QString(c)); // a keystroke each, as typed
        QCOMPARE(changed.count(), 0);

        Q_EMIT edit->editingFinished(); // focus moved on, or Return
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.constFirst().at(0).toString(), QStringLiteral("voices_path"));
        QCOMPARE(changed.constFirst().at(1).toString(), QStringLiteral("/tmp/elsewhere"));

        // Leaving the field again without touching it is not a change.
        Q_EMIT edit->editingFinished();
        QCOMPARE(changed.count(), 1);
    }

    // A dialog accepted while the value is still being typed hands it over.
    void testCommitEditsHandsOverAValueStillBeingTyped()
    {
        PluginOptionsWidget widget;
        widget.setBackend(PluginFamily::Tts, QStringLiteral("piper"));

        auto *edit = widget.findChild<QLineEdit *>(QStringLiteral("voices_path"));
        if (edit == nullptr)
            QSKIP("The Piper backend is not installed in this build");

        QSignalSpy changed(&widget, &PluginOptionsWidget::optionChanged);
        edit->selectAll();
        edit->insert(QStringLiteral("/tmp/typed"));
        QCOMPARE(changed.count(), 0);

        widget.commitEdits();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.constFirst().at(1).toString(), QStringLiteral("/tmp/typed"));

        widget.commitEdits(); // nothing new
        QCOMPARE(changed.count(), 1);
    }
};

int main(int argc, char *argv[])
{
    isolateTestSettings();

    QApplication app(argc, argv);
    PluginOptionsWidgetTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_pluginoptionswidget.moc"
