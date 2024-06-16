/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "languagedetectedtransition.h"

#include "languagebuttonswidget.h"

LanguageDetectedTransition::LanguageDetectedTransition(LanguageButtonsWidget *buttons, QState *sourceState)
    : QAbstractTransition(sourceState)
    , m_langButtons(buttons)
{
}

bool LanguageDetectedTransition::eventTest(QEvent *)
{
    return m_langButtons->checkedLanguage() != OnlineTranslator::Auto;
}

void LanguageDetectedTransition::onTransition(QEvent *)
{
}
