/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "translationedit.h"

#include "contextmenu.h"

TranslationEdit::TranslationEdit(QWidget *parent)
    : QTextEdit(parent)
{
}

void TranslationEdit::contextMenuEvent(QContextMenuEvent *event)
{
    auto *contextMenu = new ContextMenu(this, event);
    contextMenu->popup();
}
