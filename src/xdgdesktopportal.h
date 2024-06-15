/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef XDGDESKTOPPORTAL_H
#define XDGDESKTOPPORTAL_H

#include <QString>

class QWindow;

namespace XdgDesktopPortal
{
// Retrieve parent window in string form according to
// https://flatpak.github.io/xdg-desktop-portal/#parent_window
QString parentWindow(QWindow *activeWindow);
}

#endif // XDGDESKTOPPORTAL_H
