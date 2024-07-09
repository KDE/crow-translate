/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "trayicon.h"

#include "mainwindow.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QMenu>

TrayIcon::TrayIcon(MainWindow *parent)
    : QSystemTrayIcon(parent)
    , m_trayMenu(new QMenu(parent))
    , m_showMainWindowAction(m_trayMenu->addAction(QIcon::fromTheme(QStringLiteral("window-maximize")), tr("Show window"), parent, &MainWindow::open))
    , m_openSettingsAction(m_trayMenu->addAction(QIcon::fromTheme(QStringLiteral("preferences-other")), tr("Settings"), parent, &MainWindow::openSettings))
    , m_quitAction(m_trayMenu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), tr("Quit"), parent, &MainWindow::quit))
{
    setToolTip(APPLICATION_NAME);
    setContextMenu(m_trayMenu);

    connect(this, &TrayIcon::activated, [parent](QSystemTrayIcon::ActivationReason reason) {
        if (reason != QSystemTrayIcon::Trigger)
            return;

        if (parent->isActiveWindow())
            parent->hide();
        else
            parent->open();
    });
}

void TrayIcon::setTranslationNotificationTimeout(int timeout)
{
    m_translationNotificaitonTimeout = timeout;
}

void TrayIcon::retranslateMenu()
{
    m_showMainWindowAction->setText(tr("Show window"));
    m_openSettingsAction->setText(tr("Settings"));
    m_quitAction->setText(tr("Quit"));
}

void TrayIcon::showTranslationMessage(const QString &message)
{
    showMessage(tr("Translation result"), message, QSystemTrayIcon::NoIcon, m_translationNotificaitonTimeout * 1000);
}

QIcon TrayIcon::customTrayIcon(const QString &customName)
{
    if (QFileInfo::exists(customName))
        return QIcon(customName);
    return QIcon::fromTheme(customName);
}

QString TrayIcon::trayIconName(AppSettings::IconType type)
{
    switch (type) {
    case AppSettings::DefaultIcon:
        return QStringLiteral(APPLICATION_ID "-tray");
    case AppSettings::DarkIcon:
        return QStringLiteral(APPLICATION_ID "-tray-dark");
    case AppSettings::LightIcon:
        return QStringLiteral(APPLICATION_ID "-tray-light");
    default:
        return {};
    }
}
