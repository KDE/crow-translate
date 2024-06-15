/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SNIPPINGVISIBLETRANSITION_H
#define SNIPPINGVISIBLETRANSITION_H

#include <QAbstractTransition>

class SnippingArea;

class SnippingVisibleTransition : public QAbstractTransition
{
public:
    explicit SnippingVisibleTransition(SnippingArea *snippingArea, QState *sourceState = nullptr);

protected:
    bool eventTest(QEvent *) override;
    void onTransition(QEvent *) override;

private:
    SnippingArea *m_snippingarea;
};

#endif // SNIPPINGVISIBLETRANSITION_H
