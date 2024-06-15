/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "snippingvisibletransition.h"

#include "ocr/snippingarea.h"

SnippingVisibleTransition::SnippingVisibleTransition(SnippingArea *snippingArea, QState *sourceState)
    : QAbstractTransition(sourceState)
    , m_snippingarea(snippingArea)
{
}

bool SnippingVisibleTransition::eventTest(QEvent *)
{
    return m_snippingarea->isVisible();
}

void SnippingVisibleTransition::onTransition(QEvent *)
{
}
