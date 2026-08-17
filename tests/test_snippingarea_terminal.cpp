/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Regression tests for SnippingArea's terminal-signal contract: every exit
// from a snip must emit either snipped or cancelled. Before the fix,
// acceptSelection() with an empty selection (Enter without selecting, or a
// drag released without moving) hid the widget while emitting *nothing*,
// leaving a pending translate-screen-area armed for the next recognition and
// the status strip stuck on "Select a region".
//
// Driven entirely through QApplication::sendEvent() on a never-shown widget,
// so no display interaction is needed beyond QApplication itself.

#include "ocr/snippingarea.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTest>

class SnippingAreaTerminalTest : public QObject
{
    Q_OBJECT

private slots:
    void testEnterWithEmptySelectionEmitsCancelled();
    void testEscapeEmitsCancelled();
    void testDragReleasedWithoutMoveEmitsCancelled();

private:
    static void sendKey(QWidget *receiver, int key);
    static void sendMouse(QWidget *receiver, QEvent::Type type, const QPoint &pos);
};

// Enter with no selection made: acceptSelection() takes the empty branch.
void SnippingAreaTerminalTest::testEnterWithEmptySelectionEmitsCancelled()
{
    SnippingArea area;
    area.setRegionRememberType(AppSettings::NeverRemember);

    QSignalSpy cancelledSpy(&area, &SnippingArea::cancelled);
    sendKey(&area, Qt::Key_Return);

    QCOMPARE(cancelledSpy.count(), 1);
}

// The long-standing Escape path, pinned so the "always a terminal" contract
// has both halves locked down.
void SnippingAreaTerminalTest::testEscapeEmitsCancelled()
{
    SnippingArea area;

    QSignalSpy cancelledSpy(&area, &SnippingArea::cancelled);
    sendKey(&area, Qt::Key_Escape);

    QCOMPARE(cancelledSpy.count(), 1);
}

// Start a region drag and release without moving: mouseReleaseEvent takes the
// Outside + confirmOnRelease path into acceptSelection() with an empty
// selection - the manual "click without drag" scenario, driven headlessly.
void SnippingAreaTerminalTest::testDragReleasedWithoutMoveEmitsCancelled()
{
    SnippingArea area;
    area.setRegionRememberType(AppSettings::NeverRemember);
    area.setCaptureOnRelese(true);

    QSignalSpy cancelledSpy(&area, &SnippingArea::cancelled);
    sendMouse(&area, QEvent::MouseButtonPress, QPoint(100, 100));
    sendMouse(&area, QEvent::MouseButtonRelease, QPoint(100, 100));

    QCOMPARE(cancelledSpy.count(), 1);
}

void SnippingAreaTerminalTest::sendKey(QWidget *receiver, int key)
{
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
    QApplication::sendEvent(receiver, &press);
    QApplication::sendEvent(receiver, &release);
}

void SnippingAreaTerminalTest::sendMouse(QWidget *receiver, QEvent::Type type, const QPoint &pos)
{
    const QPointF local(pos);
    const QPointF global(receiver->mapToGlobal(pos));
    QMouseEvent event(type, local, global, Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(receiver, &event);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    SnippingAreaTerminalTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_snippingarea_terminal.moc"
