/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "trayicon.h"

#include "mainwindow.h"

#include <QAction>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenu>
#include <QSysInfo>
#include <QUrl>
#include <QUrlQuery>

TrayIcon::TrayIcon(MainWindow *parent)
    : QSystemTrayIcon(parent)
    , m_trayMenu(new QMenu(parent))
    , m_showMainWindowAction(m_trayMenu->addAction(QIcon::fromTheme(QStringLiteral(APPLICATION_ID "-tray")), tr("Show window"), parent, &MainWindow::open))
    , m_openSettingsAction(m_trayMenu->addAction(QIcon::fromTheme(QStringLiteral("preferences-other")), tr("Settings"), parent, &MainWindow::openSettings))
    , m_quitAction(m_trayMenu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), tr("Quit"), parent, &MainWindow::quit))
{
    m_reportBugAction = m_trayMenu->addAction(tr("Report Bug…"));
    connect(m_reportBugAction, &QAction::triggered, this, []() {
        QUrl url(QStringLiteral("https://bugs.kde.org/enter_bug.cgi"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("product"), QStringLiteral("Crow Translate"));
        query.addQueryItem(QStringLiteral("component"), QStringLiteral("general"));
        query.addQueryItem(QStringLiteral("format"), QStringLiteral("__default__"));
        query.addQueryItem(QStringLiteral("short_desc"), QString());
        query.addQueryItem(QStringLiteral("comment"),
                           QStringLiteral("Version: %1\nOperating system: %2\n\nWhat happened and what did you expect?\n").arg(QCoreApplication::applicationVersion(), QSysInfo::prettyProductName()));
        url.setQuery(query);
        QDesktopServices::openUrl(url);
    });

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
    m_reportBugAction->setText(tr("Report Bug…"));
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
