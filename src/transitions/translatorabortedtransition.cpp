/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "translatorabortedtransition.h"

#include "qonlinetranslator.h"

TranslatorAbortedTransition::TranslatorAbortedTransition(QOnlineTranslator *translator, QState *sourceState)
    : QSignalTransition(translator, &QOnlineTranslator::finished, sourceState)
    , m_translator(translator)
{
}

bool TranslatorAbortedTransition::eventTest(QEvent *event)
{
    return !m_translator->isRunning() || QSignalTransition::eventTest(event);
}
