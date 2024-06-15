/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "genericscreengrabber.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

GenericScreenGrabber::GenericScreenGrabber(QObject *parent)
    : AbstractScreenGrabber(parent)
{
}

void GenericScreenGrabber::grab()
{
    QMap<const QScreen *, QImage> images;
    for (QScreen *screen : QGuiApplication::screens())
        images.insert(screen, screen->grabWindow(0).toImage());
    emit grabbed(images);
}

void GenericScreenGrabber::cancel()
{
}
