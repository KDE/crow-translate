/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SHORTCUTSVIEW_H
#define SHORTCUTSVIEW_H

#include <QTreeView>

class ShortcutItem;
class ShortcutsModel;

class ShortcutsView : public QTreeView
{
    Q_OBJECT
    Q_DISABLE_COPY(ShortcutsView)

public:
    explicit ShortcutsView(QWidget *parent = nullptr);

    ShortcutsModel *model() const;
    ShortcutItem *currentItem() const;

signals:
    void currentItemChanged(ShortcutItem *shortcut);
};

#endif // SHORTCUTSVIEW_H
