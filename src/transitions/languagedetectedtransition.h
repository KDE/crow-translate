/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LANGUAGEDETECTEDTRANSITION_H
#define LANGUAGEDETECTEDTRANSITION_H

#include <QAbstractTransition>

class LanguageButtonsWidget;

class LanguageDetectedTransition : public QAbstractTransition
{
public:
    explicit LanguageDetectedTransition(LanguageButtonsWidget *buttons, QState *sourceState = nullptr);

protected:
    bool eventTest(QEvent *) override;
    void onTransition(QEvent *) override;

private:
    LanguageButtonsWidget *m_langButtons;
};

#endif // LANGUAGEDETECTEDTRANSITION_H
