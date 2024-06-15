/*
 *  Copyright © 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 *  Copyright © 2022 Volk Milit <javirrdar@gmail.com>
 *
 *  This file is part of Crow Translate.
 *
 *  Crow Translate is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Crow Translate is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crow Translate. If not, see <https://www.gnu.org/licenses/>.
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
