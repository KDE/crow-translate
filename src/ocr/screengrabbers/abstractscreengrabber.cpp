/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abstractscreengrabber.h"

#include "genericscreengrabber.h"

#include <QMessageBox>

#ifdef Q_OS_LINUX
#include "waylandgnomescreengrabber.h"
#include "waylandplasmascreengrabber.h"
#include "waylandportalscreengrabber.h"

#include <QX11Info>
#endif

AbstractScreenGrabber::AbstractScreenGrabber(QObject *parent)
    : QObject(parent)
{
}

AbstractScreenGrabber *AbstractScreenGrabber::createScreenGrabber(QObject *parent)
{
#ifdef Q_OS_LINUX
    if (!QX11Info::isPlatformX11()) {
        if (WaylandGnomeScreenGrabber::isAvailable())
            return new WaylandGnomeScreenGrabber(parent);
        if (WaylandPortalScreenGrabber::isAvailable())
            return new WaylandPortalScreenGrabber(parent);
        if (WaylandPlasmaScreenGrabber::isAvailable())
            return new WaylandPlasmaScreenGrabber(parent);
    }
#endif
    return new GenericScreenGrabber(parent);
}

void AbstractScreenGrabber::showError(const QString &errorString)
{
    QMessageBox message;
    message.setIcon(QMessageBox::Critical);
    message.setText(tr("Unable to grab screen"));
    message.setInformativeText(errorString);
    message.exec();

    emit grabbingFailed();
}
