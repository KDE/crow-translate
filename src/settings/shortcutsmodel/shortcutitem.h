/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SHORTCUTITEM_H
#define SHORTCUTITEM_H

#include <QIcon>
#include <QKeySequence>

class ShortcutsModel;

class ShortcutItem
{
    Q_DISABLE_COPY(ShortcutItem)

public:
    explicit ShortcutItem(ShortcutsModel *model);
    ShortcutItem(QString description, ShortcutItem *parent);
    ShortcutItem(QString description, const QString &iconName, ShortcutItem *parent);
    ~ShortcutItem();

    ShortcutItem *child(int row);
    int childCount() const;
    int row() const;
    ShortcutItem *parentItem();

    QString description() const;
    QIcon icon() const;

    QKeySequence defaultShortcut() const;
    void setDefaultShortcut(const QKeySequence &shortcut);

    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence &shortcut);
    void resetShortcut();
    void resetAllShortucts();

    bool isEnabled() const;
    void setEnabled(bool enabled);

private:
    QString m_description;
    QIcon m_icon;
    QKeySequence m_shortcut;
    QKeySequence m_defaultShortcut;
    bool m_enabled = true;

    QVector<ShortcutItem *> m_childItems;
    ShortcutItem *m_parentItem = nullptr;
    ShortcutsModel *m_model;
};

#endif // SHORTCUTITEM_H
