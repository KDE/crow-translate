/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRAYICON_H
#define TRAYICON_H

#include "settings/appsettings.h"

#include <QSystemTrayIcon>

class QAction;
class MainWindow;

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT
    Q_DISABLE_COPY(TrayIcon)

public:
    explicit TrayIcon(MainWindow *parent = nullptr);

    void setTranslationNotificationTimeout(int timeout);
    void retranslateMenu();
    void showTranslationMessage(const QString &message);

    static QIcon customTrayIcon(const QString &customName);
    static QString trayIconName(AppSettings::IconType type);

private:
    QMenu *m_trayMenu;
    QAction *m_showMainWindowAction;
    QAction *m_openSettingsAction;
    QAction *m_quitAction;

    int m_translationNotificaitonTimeout = AppSettings::defaultTranslationNotificationTimeout();
};

#endif // TRAYICON_H
