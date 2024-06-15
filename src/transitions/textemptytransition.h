/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TEXTEMPTYTRANSITION_H
#define TEXTEMPTYTRANSITION_H

#include <QAbstractTransition>

class SourceTextEdit;

class TextEmptyTransition : public QAbstractTransition
{
public:
    explicit TextEmptyTransition(SourceTextEdit *edit, QState *sourceState = nullptr);

protected:
    bool eventTest(QEvent *) override;
    void onTransition(QEvent *) override;

private:
    SourceTextEdit *m_edit;
};

#endif // TEXTEMPTYTRANSITION_H
