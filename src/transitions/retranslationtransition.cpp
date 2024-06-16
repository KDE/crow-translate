/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "retranslationtransition.h"

#include "languagebuttonswidget.h"
#include "onlinetranslator/onlinetranslator.h"

RetranslationTransition::RetranslationTransition(OnlineTranslator *translator, LanguageButtonsWidget *buttons, QState *sourceState)
    : QAbstractTransition(sourceState)
    , m_translator(translator)
    , m_langButtons(buttons)
{
}

bool RetranslationTransition::eventTest(QEvent *)
{
    return m_translator->error() == OnlineTranslator::NoError
        && m_langButtons->isAutoButtonChecked()
        && m_translator->sourceLanguage() == m_translator->translationLanguage();
}

void RetranslationTransition::onTransition(QEvent *)
{
}
