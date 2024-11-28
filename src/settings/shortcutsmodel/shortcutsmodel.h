/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SHORTCUTSMODEL_H
#define SHORTCUTSMODEL_H

#include <QAbstractItemModel>

class ShortcutItem;
class AppSettings;

class ShortcutsModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY(ShortcutsModel)

public:
    enum Column {
        DescriptionColumn,
        ShortcutColumn
    };
    Q_ENUM(Column)

    explicit ShortcutsModel(QObject *parent = nullptr);
    ~ShortcutsModel() override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void loadShortcuts(const AppSettings &settings);
    void saveShortcuts(AppSettings &settings) const;
    void resetAllShortcuts();

    void updateShortcut(ShortcutItem *item);
    void updateItem(ShortcutItem *item);

public slots:
    void setGlobalShortuctsEnabled(bool enabled);

private:
    QModelIndex index(ShortcutItem *item, int column) const;

    ShortcutItem *m_rootItem;
    ShortcutItem *m_globalShortcuts;

    // Global shortcuts
    ShortcutItem *m_translateSelectionShortcut;
    ShortcutItem *m_speakSelectionShortcut;
    ShortcutItem *m_speakTranslatedSelectionShortcut;
    ShortcutItem *m_stopSpeakingShortcut;
    ShortcutItem *m_playPauseSpeakingShortcut;
    ShortcutItem *m_showMainWindowShortcut;
    ShortcutItem *m_copyTranslatedSelectionShortcut;
    ShortcutItem *m_recognizeScreenAreaShortcut;
    ShortcutItem *m_translateScreenAreaShortcut;
    ShortcutItem *m_delayedRecognizeScreenAreaShortcut;
    ShortcutItem *m_delayedTranslateScreenAreaShortcut;
    ShortcutItem *m_toggleOcrNegateShortcut;

    // Window shortcuts
    ShortcutItem *m_translateShortcut;
    ShortcutItem *m_swapShortcut;
    ShortcutItem *m_closeWindowShortcut;

    // Source text shortcuts
    ShortcutItem *m_speakSourceShortcut;

    // Translation text shortcuts
    ShortcutItem *m_speakTranslationShortcut;
    ShortcutItem *m_copyTranslationShortcut;
};

#endif // SHORTCUTSMODEL_H
