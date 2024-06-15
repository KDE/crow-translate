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

#include "retranslationtransition.h"

#include "languagebuttonswidget.h"
#include "qonlinetranslator.h"

RetranslationTransition::RetranslationTransition(QOnlineTranslator *translator, LanguageButtonsWidget *buttons, QState *sourceState)
    : QAbstractTransition(sourceState)
    , m_translator(translator)
    , m_langButtons(buttons)
{
}

bool RetranslationTransition::eventTest(QEvent *)
{
    return m_translator->error() == QOnlineTranslator::NoError
        && m_langButtons->isAutoButtonChecked()
        && m_translator->sourceLanguage() == m_translator->translationLanguage();
}

void RetranslationTransition::onTransition(QEvent *)
{
}
