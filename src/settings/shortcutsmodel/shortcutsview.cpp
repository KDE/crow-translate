/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "shortcutsview.h"

#include "shortcutsmodel.h"

#include <QHeaderView>

ShortcutsView::ShortcutsView(QWidget *parent)
    : QTreeView(parent)
{
    QTreeView::setModel(new ShortcutsModel(this));
    expandAll();
    header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(selectionModel(), &QItemSelectionModel::currentChanged, [this](const QModelIndex &current) {
        auto *shortcut = static_cast<ShortcutItem *>(current.internalPointer());
        emit currentItemChanged(shortcut);
    });
}

ShortcutsModel *ShortcutsView::model() const
{
    return qobject_cast<ShortcutsModel *>(QTreeView::model());
}

ShortcutItem *ShortcutsView::currentItem() const
{
    return static_cast<ShortcutItem *>(currentIndex().internalPointer());
}
