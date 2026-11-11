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

Qt::ScreenOrientation ScreenWatcher::layoutOrientation(Qt::ScreenOrientation wanted, Qt::ScreenOrientation reported, Qt::ScreenOrientation primary)
{
    for (const Qt::ScreenOrientation candidate : {wanted, reported}) {
        switch (candidate) {
        case Qt::LandscapeOrientation:
        case Qt::PortraitOrientation:
        case Qt::InvertedLandscapeOrientation:
        case Qt::InvertedPortraitOrientation:
            return candidate;
        default:
            // Qt::PrimaryOrientation, or a value that is none of them (the setting is read
            // from a file): not an answer, ask the next one.
            break;
        }
    }
    return primary == Qt::PortraitOrientation ? Qt::PortraitOrientation : Qt::LandscapeOrientation;
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
        if (widget != nullptr && widget->screen() == screen) {
            emit screenOrientationChanged(orientation);
        }
        // clang-format on
    });
}
