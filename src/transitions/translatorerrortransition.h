/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATORERRORTRANSITION_H
#define TRANSLATORERRORTRANSITION_H

#include <QAbstractTransition>

class OnlineTranslator;

class TranslatorErrorTransition : public QAbstractTransition
{
public:
    explicit TranslatorErrorTransition(OnlineTranslator *translator, QState *sourceState = nullptr);

protected:
    bool eventTest(QEvent *) override;
    void onTransition(QEvent *) override;

private:
    OnlineTranslator *m_translator;
};

#endif // TRANSLATORERRORTRANSITION_H
