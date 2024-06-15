/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ABSTRACTSCREENGRABBER_H
#define ABSTRACTSCREENGRABBER_H

#include <QObject>

class QScreen;

class AbstractScreenGrabber : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AbstractScreenGrabber)

public:
    static AbstractScreenGrabber *createScreenGrabber(QObject *parent = nullptr);

signals:
    void grabbed(const QMap<const QScreen *, QImage> &screenImages);
    void grabbingFailed();

public slots:
    virtual void grab() = 0;
    virtual void cancel() = 0;

protected:
    explicit AbstractScreenGrabber(QObject *parent = nullptr);

protected slots:
    void showError(const QString &errorString);
};

#endif // ABSTRACTSCREENGRABBER_H
