/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "textemptytransition.h"

#include "sourcetextedit.h"

TextEmptyTransition::TextEmptyTransition(SourceTextEdit *edit, QState *sourceState)
    : QAbstractTransition(sourceState)
    , m_edit(edit)
{
}

bool TextEmptyTransition::eventTest(QEvent * /*event*/)
{
    return m_edit->toSourceText().isEmpty();
}

void TextEmptyTransition::onTransition(QEvent * /*event*/)
{
}
