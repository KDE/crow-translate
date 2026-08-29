/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Contract tests for UserNotifier, the channel that replaced the
// QMessageBox::exec() calls the providers used to make themselves.
//
// Headless on purpose: the whole point of the change is that reporting a
// provider failure no longer needs a widget toolkit, so a test for it must
// not need one either. QCoreApplication, no display.

#include "core/usernotifier.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

class UserNotifierTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<UserNotifier::Notification>();
    }

    void testPayloadSurvivesDelivery()
    {
        QSignalSpy spy(UserNotifier::instance(), &UserNotifier::notified);
        QVERIFY(spy.isValid());

        UserNotifier::Notification sent;
        sent.severity = UserNotifier::Severity::Warning;
        sent.title = QStringLiteral("Piper Voice Models Required");
        sent.text = QStringLiteral("No voice models were found.");
        sent.details = QStringLiteral("<p>Download them from somewhere.</p>");
        sent.detailsAreRichText = true;
        sent.requiresAcknowledgement = true;
        UserNotifier::notify(sent);

        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.count(), 1);

        const auto notification = spy.at(0).at(0).value<UserNotifier::Notification>();
        QCOMPARE(notification.severity, UserNotifier::Severity::Warning);
        QCOMPARE(notification.title, QStringLiteral("Piper Voice Models Required"));
        QCOMPARE(notification.text, QStringLiteral("No voice models were found."));
        QCOMPARE(notification.details, QStringLiteral("<p>Download them from somewhere.</p>"));
        QVERIFY(notification.detailsAreRichText);
        QVERIFY(notification.requiresAcknowledgement);
    }

    // notify() must not re-enter its caller. Two of the call sites are static
    // factory functions running inside a frontend's constructor; a direct
    // emission would run frontend code against a half-built object.
    void testDeliveryIsQueuedNotImmediate()
    {
        bool arrived = false;
        const QMetaObject::Connection connection =
            connect(UserNotifier::instance(), &UserNotifier::notified, this, [&arrived](const UserNotifier::Notification &) {
                arrived = true;
            });

        UserNotifier::Notification sent;
        sent.text = QStringLiteral("later, not now");
        UserNotifier::notify(sent);
        QVERIFY2(!arrived, "notify() delivered synchronously, re-entering its caller");

        QTRY_VERIFY_WITH_TIMEOUT(arrived, 5000);
        disconnect(connection);
    }

    // The property that let the QTimer::singleShot(100, ...) hack go away.
    // MainWindow connects at the top of its constructor and builds the
    // providers that may notify further down the same constructor; because
    // delivery waits for the stack to unwind, a subscriber connected after
    // the notify() call still receives it. The old code guessed at 100ms
    // instead.
    void testSubscriberConnectedAfterNotifyStillReceives()
    {
        UserNotifier::Notification sent;
        sent.text = QStringLiteral("emitted before anyone was listening");
        UserNotifier::notify(sent);

        QSignalSpy spy(UserNotifier::instance(), &UserNotifier::notified);
        QVERIFY(spy.isValid());

        QVERIFY2(spy.wait(5000), "a notification raised before the frontend connected was lost");
        QCOMPARE(spy.at(0).at(0).value<UserNotifier::Notification>().text, QStringLiteral("emitted before anyone was listening"));
    }
};

QTEST_GUILESS_MAIN(UserNotifierTest)

#include "test_usernotifier.moc"
