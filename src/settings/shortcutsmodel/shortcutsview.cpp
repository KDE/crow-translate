/*
 *  Copyright © 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 *  Copyright © 2022 Volk Milit <javirrdar@gmail.com>
 *
 *  This file is part of Crow Translate.
 *
 *  Crow Translate is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Crow Translate is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crow Translate. If not, see <https://www.gnu.org/licenses/>.
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
