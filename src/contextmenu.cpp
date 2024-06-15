/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "contextmenu.h"

#include <QDesktopServices>

ContextMenu::~ContextMenu()
{
    delete m_menu;
}

void ContextMenu::popup()
{
    m_menu->popup(m_menu->pos());
    connect(m_menu, &QMenu::aboutToHide, this, &ContextMenu::deleteLater);
}

void ContextMenu::searchOnForvo()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://forvo.com/search/%1/").arg(m_text)));
}
