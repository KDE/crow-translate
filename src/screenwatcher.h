/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SCREENWATCHER_H
#define SCREENWATCHER_H

#include <QObject>

class QScreen;

class ScreenWatcher : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ScreenWatcher)

public:
    explicit ScreenWatcher(QWidget *parent);

    static bool isWidthFitScreen(QWidget *widget);

signals:
    void screenOrientationChanged(Qt::ScreenOrientation orientation);

private slots:
    void listenForOrientationChange(QScreen *screen);
};

#endif // SCREENWATCHER_H
