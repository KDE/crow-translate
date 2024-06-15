/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef OCRUNINITIALIZEDTRANSITION_H
#define OCRUNINITIALIZEDTRANSITION_H

#include <QAbstractTransition>

class MainWindow;

class OcrUninitializedTransition : public QAbstractTransition
{
public:
    explicit OcrUninitializedTransition(MainWindow *mainWindow, QState *sourceState = nullptr);

protected:
    bool eventTest(QEvent *) override;
    void onTransition(QEvent *) override;

private:
    MainWindow *m_mainWindow;
};

#endif // OCRUNINITIALIZEDTRANSITION_H
