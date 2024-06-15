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

#ifndef TRANSLATORABORTEDTRANSITION_H
#define TRANSLATORABORTEDTRANSITION_H

#include <QSignalTransition>

class QOnlineTranslator;

class TranslatorAbortedTransition : public QSignalTransition
{
public:
    explicit TranslatorAbortedTransition(QOnlineTranslator *translator, QState *sourceState = nullptr);

protected:
    bool eventTest(QEvent *event) override;

private:
    QOnlineTranslator *m_translator;
};

#endif // TRANSLATORABORTEDTRANSITION_H
