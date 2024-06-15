/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RETRANSLATIONTRANSITION_H
#define RETRANSLATIONTRANSITION_H

#include <QAbstractTransition>

class QOnlineTranslator;
class LanguageButtonsWidget;

class RetranslationTransition : public QAbstractTransition
{
public:
    RetranslationTransition(QOnlineTranslator *translator, LanguageButtonsWidget *buttons, QState *sourceState = nullptr);

protected:
    bool eventTest(QEvent *) override;
    void onTransition(QEvent *) override;

private:
    QOnlineTranslator *m_translator;
    LanguageButtonsWidget *m_langButtons;
};

#endif // RETRANSLATIONTRANSITION_H
