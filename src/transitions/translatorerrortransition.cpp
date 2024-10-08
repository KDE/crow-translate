/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "translatorerrortransition.h"

#include "qonlinetranslator.h"

TranslatorErrorTransition::TranslatorErrorTransition(QOnlineTranslator *translator, QState *sourceState)
    : QAbstractTransition(sourceState)
    , m_translator(translator)
{
}

bool TranslatorErrorTransition::eventTest(QEvent *)
{
    return m_translator->error() != QOnlineTranslator::NoError;
}

void TranslatorErrorTransition::onTransition(QEvent *)
{
}
