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
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#include <QWindow>
#endif

ScreenWatcher::ScreenWatcher(QWidget *parent)
    : QObject(parent)
{
    for (QScreen *screen : QGuiApplication::screens())
        listenForOrientationChange(screen);
    connect(qobject_cast<QGuiApplication *>(QCoreApplication::instance()), &QGuiApplication::screenAdded, this, &ScreenWatcher::listenForOrientationChange);
}

bool ScreenWatcher::isWidthFitScreen(QWidget *widget)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return widget->frameGeometry().width() <= widget->screen()->availableGeometry().width();
#else
    if (!widget->windowHandle())
        widget->winId(); // Call to create handle
    return widget->frameGeometry().width() <= widget->windowHandle()->screen()->availableGeometry().width();
#endif
}

void ScreenWatcher::listenForOrientationChange(QScreen *screen)
{
    connect(screen, &QScreen::orientationChanged, [this, screen](Qt::ScreenOrientation orientation) {
        auto *widget = qobject_cast<QWidget *>(parent());
        // clang-format off
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        if (widget->screen() == screen) {
#else
        if (widget->windowHandle() && widget->windowHandle()->screen() == screen) {
#endif
            emit screenOrientationChanged(orientation);
        }
        // clang-format on
    });
    screen->setOrientationUpdateMask(Qt::LandscapeOrientation
                                     | Qt::PortraitOrientation
                                     | Qt::InvertedLandscapeOrientation
                                     | Qt::InvertedPortraitOrientation);
}
