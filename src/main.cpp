/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cli.h"
#include "cmake.h"
#include "mainwindow.h"
#include "singleapplication.h"

#ifdef Q_OS_UNIX
#include "ocr/ocr.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QtCore>
#endif

int launchGui(int argc, char *argv[]);
int launchCli(int argc, char *argv[]);
#ifdef Q_OS_UNIX
void registerDBusObject(QObject *object);
#endif

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationVersion(QStringLiteral("%1.%2.%3").arg(VERSION_MAJOR).arg(VERSION_MINOR).arg(VERSION_PATCH));
    QCoreApplication::setApplicationName(QStringLiteral(PROJECT_NAME));
    QCoreApplication::setOrganizationName(QStringLiteral(PROJECT_NAME));

    if (argc == 1)
        return launchGui(argc, argv); // Launch GUI if there are no arguments

    return launchCli(argc, argv);
}

int launchGui(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && defined(Q_OS_WIN)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#if defined(Q_OS_LINUX)
    QGuiApplication::setDesktopFileName(QStringLiteral(DESKTOP_FILE));
#elif defined(Q_OS_WIN) || defined(Q_OS_DARWIN)
    QIcon::setThemeName("hicolor");
#endif

    SingleApplication app(argc, argv);

    AppSettings settings;
    settings.setupLocalization();
    if (settings.instanceUrl().isEmpty()) {
        settings.setBestInstance();
    }

    MainWindow window;

#ifdef Q_OS_UNIX
    if (QDBusConnection::sessionBus().isConnected()) {
        const QString service = QStringLiteral(APPLICATION_ID);
        if (QDBusConnection::sessionBus().registerService(service)) {
            registerDBusObject(&window);
            registerDBusObject(window.ocr());
        } else {
            qWarning() << QCoreApplication::translate("D-Bus", "D-Bus service %1 is already registered by another application").arg(service);
        }
    }
#endif

    return QCoreApplication::exec();
}

int launchCli(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    AppSettings().setupLocalization();

    Cli cli;
    cli.process(app);

    return QCoreApplication::exec();
}

#ifdef Q_OS_UNIX
void registerDBusObject(QObject *object)
{
    const QString objectPath = QStringLiteral("/%1/").arg(QStringLiteral(APPLICATION_ID).replace('.', '/'));
    if (!QDBusConnection::sessionBus().registerObject(objectPath + object->metaObject()->className(), object, QDBusConnection::ExportScriptableSlots))
        qWarning() << QCoreApplication::translate("D-Bus", "Unable to register D-Bus object for %1").arg(object->metaObject()->className());
}
#endif
