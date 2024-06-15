/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CONTEXTMENU_H
#define CONTEXTMENU_H

#include "translationedit.h"

#include <QContextMenuEvent>
#include <QMenu>

class QContextMenuEvent;

class ContextMenu : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ContextMenu)

public:
    template<class TextEdit>
    ContextMenu(TextEdit *edit, const QContextMenuEvent *event)
        : QObject(edit)
    {
        m_text = edit->textCursor().selectedText();
        if (m_text.isEmpty()) {
            if constexpr (std::is_same_v<TextEdit, TranslationEdit>)
                m_text = edit->translation();
            else
                m_text = edit->toPlainText();
        }

        m_menu = edit->createStandardContextMenu(event->globalPos());
        m_menu->move(event->globalPos());
        m_menu->addSeparator();
        QAction *searchOnForvoAction = m_menu->addAction(QIcon::fromTheme("text-speak"), tr("Search on Forvo.com"), this, &ContextMenu::searchOnForvo);
        if (m_text.isEmpty())
            searchOnForvoAction->setEnabled(false);
    }

    ~ContextMenu() override;

    void popup();

private slots:
    void searchOnForvo();

private:
    QMenu *m_menu;
    QString m_text;
};

#endif // CONTEXTMENU_H
