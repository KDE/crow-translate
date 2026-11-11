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

    // The orientation a window is laid out for: the one asked for or, when that is left to the
    // screen (Qt::PrimaryOrientation), the one the screen reports. A platform that does not
    // know how its screen is turned reports Qt::PrimaryOrientation as well - the offscreen and
    // VNC platforms do, and a plain framebuffer - and then the screen's shape decides, which is
    // what `primary` (QScreen::primaryOrientation()) is. Never answers Qt::PrimaryOrientation.
    static Qt::ScreenOrientation layoutOrientation(Qt::ScreenOrientation wanted, Qt::ScreenOrientation reported, Qt::ScreenOrientation primary);

signals:
    void screenOrientationChanged(Qt::ScreenOrientation orientation);

private slots:
    void listenForOrientationChange(QScreen *screen);
};

#endif // SCREENWATCHER_H
