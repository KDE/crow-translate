/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DBUSSCREENGRABBER_H
#define DBUSSCREENGRABBER_H

#include "abstractscreengrabber.h"

#include <QDBusPendingReply>

class QDBusPendingCallWatcher;

class DBusScreenGrabber : public AbstractScreenGrabber
{
    Q_OBJECT
    Q_DISABLE_COPY(DBusScreenGrabber)

public slots:
    void cancel() override;

protected:
    explicit DBusScreenGrabber(QObject *parent = nullptr);

    template<class T>
    QDBusPendingReply<T> readReply()
    {
        QDBusPendingReply<T> reply = *m_callWatcher;
        m_callWatcher->deleteLater();
        m_callWatcher = nullptr;
        return reply;
    }

    static QMap<const QScreen *, QImage> splitScreenImages(const QPixmap &pixmap);

    QDBusPendingCallWatcher *m_callWatcher;
};

#endif // DBUSSCREENGRABBER_H
