/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef USERNOTIFIER_H
#define USERNOTIFIER_H

#include <QObject>
#include <QString>

// The one way the core tells the user something outside the normal flow of a
// translation: a provider that would not start, a voice pack that is missing,
// a screen that could not be grabbed.
//
// These used to be QMessageBox::exec() calls made from inside the providers
// themselves, which is why src/translator, src/tts and src/ocr all linked
// QtWidgets. That is wrong three times over. A backend decided how a message
// should look, so there was no way for a non-widget frontend to show it at
// all - the CLI ran the same code paths and simply constructed dialogs nobody
// would ever see. A modal exec() from inside a factory function blocks
// whatever called it. And it forces every future frontend, and every provider
// that becomes a loadable plugin, to link a widget toolkit.
//
// The core says what happened; the frontend decides what that looks like.
// The GUI puts up a message box, the CLI prints to stderr.
//
// Delivery is queued, so notify() never re-enters its caller and a frontend
// that connects later in the same call stack - MainWindow does, while
// constructing the providers that may notify - still receives it.
class UserNotifier : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(UserNotifier)

public:
    enum class Severity : uint8_t {
        Information,
        Warning,
        Critical
    };
    Q_ENUM(Severity)

    struct Notification {
        Severity severity = Severity::Information;
        QString title;
        // Short summary. Always plain text: it is the line a terminal prints.
        QString text;
        // Optional longer body. May be rich text - several of these are
        // step-by-step instructions with links, written as HTML long before
        // this class existed.
        QString details;
        bool detailsAreRichText = false;
        // The user is expected to see and dismiss this one before carrying on
        // - a privacy notice, not a hint. A widget frontend shows it modally;
        // one without windows has nothing to do differently.
        bool requiresAcknowledgement = false;
    };

    static UserNotifier *instance();
    static void notify(const Notification &notification);

signals:
    void notified(const UserNotifier::Notification &notification);

private:
    explicit UserNotifier(QObject *parent = nullptr);
};

Q_DECLARE_METATYPE(UserNotifier::Notification)

#endif // USERNOTIFIER_H
