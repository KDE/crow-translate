/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRANSLATIONEDIT_H
#define TRANSLATIONEDIT_H

#include <QTextEdit>

class TranslationEdit : public QTextEdit
{
    Q_OBJECT
    Q_DISABLE_COPY(TranslationEdit)

public:
    explicit TranslationEdit(QWidget *parent = nullptr);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
};

#endif // TRANSLATIONEDIT_H
