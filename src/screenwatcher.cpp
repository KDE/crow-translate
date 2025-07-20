/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "screenwatcher.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

ScreenWatcher::ScreenWatcher(QWidget *parent)
    : QObject(parent)
{
    for (QScreen *screen : QGuiApplication::screens())
        listenForOrientationChange(screen);
    connect(qobject_cast<QGuiApplication *>(QCoreApplication::instance()), &QGuiApplication::screenAdded, this, &ScreenWatcher::listenForOrientationChange);
}

bool ScreenWatcher::isWidthFitScreen(QWidget *widget)
{
    return widget->frameGeometry().width() <= widget->screen()->availableGeometry().width();
}

void ScreenWatcher::listenForOrientationChange(QScreen *screen)
{
    connect(screen, &QScreen::orientationChanged, [this, screen](Qt::ScreenOrientation orientation) {
        switch (orientation) {
        case Qt::LandscapeOrientation:
        case Qt::PortraitOrientation:
        case Qt::InvertedLandscapeOrientation:
        case Qt::InvertedPortraitOrientation:
            break;
        default:
            return;
        }
        auto *widget = qobject_cast<QWidget *>(parent());
        // clang-format off
        if (widget->screen() == screen) {
            emit screenOrientationChanged(orientation);
        }
        // clang-format on
    });
}
