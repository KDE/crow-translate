/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cli.h"
#include "cmake.h"
#include "iconutils.h"
#include "instancepingerdialog.h"
#include "language.h"
#include "mainwindow.h"
#include "singleapplication.h"
#include "settings/appsettings.h"

#include <QTimer>

#ifdef WITH_KICONTHEMES
#include <KIconTheme>
#endif

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef Q_OS_UNIX
#include "ocr/tesseractocr.h"

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
    Q_INIT_RESOURCE(engines);
    Q_INIT_RESOURCE(icon_theme);
#if defined(Q_OS_WIN) || defined(Q_OS_DARWIN)
    // app.qrc (which holds the application icon) is only compiled into the
    // STATIC crow-translate-lib on Windows and macOS (see the APPLE/WIN32
    // target_sources blocks in CMakeLists.txt). Nothing references the
    // resource initializer from the executable, so the linker drops it and
    // QIcon::fromTheme() cannot find :/icons/hicolor. Register it here.
    Q_INIT_RESOURCE(app);
#endif

#ifdef WITH_KICONTHEMES
    // Set up KDE icon theming before the application object exists (required;
    // it installs a startup hook). This makes QIcon::fromTheme() calls go
    // through KIconEngine, which resolves icons through the full KDE theme
    // chain (configured theme -> its Inherits -> fallback), so names that only
    // exist in newer themes (e.g. "edit-clear-all" under Oxygen, bug 509329)
    // still resolve. On the KDE platform theme it is a no-op.
    KIconTheme::initTheme();
#endif

#if defined(Q_OS_WIN)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    // Request per-monitor DPI awareness (V2) explicitly so that the screen
    // geometry and grabbed images reported by Qt use a single, consistent scale
    // regardless of any application manifest. Without this, on a scaled main
    // monitor alongside a second monitor the capture area maps its coordinates
    // incorrectly and the grabbed pixmap is offset or clipped.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
#if defined(Q_OS_LINUX)
    QGuiApplication::setDesktopFileName(QStringLiteral(DESKTOP_FILE_BASENAME));
#elif defined(Q_OS_WIN) || defined(Q_OS_DARWIN)
    QIcon::setThemeName("hicolor");
#endif

    const SingleApplication app(argc, argv);

    // KIconLoader::global() requires a live QGuiApplication; set the window
    // icon only after the application object exists (loading it earlier
    // crashes in the color-scheme lookup on a null application).
    QGuiApplication::setWindowIcon(IconUtils::loadDesktop(QStringLiteral(APPLICATION_ID)));

    AppSettings settings;
    settings.loadCustomLanguageRegistry(); // Load persisted custom languages
    Language::setCustomLanguageRegistryChangedCallback(&AppSettings::onCustomLanguageRegistryChanged);
    settings.setupLocalization();
    if (settings.instance().isEmpty()) {
        InstancePingerDialog instancePingerDialog;
        instancePingerDialog.exec();
        settings.setInstance(instancePingerDialog.fastestUrl());
    }

    MainWindow window;

#ifdef Q_OS_UNIX
    if (QDBusConnection::sessionBus().isConnected()) {
        const QString service = QStringLiteral(APPLICATION_ID);
        qDebug() << "Attempting to register D-Bus service:" << service;
        if (QDBusConnection::sessionBus().registerService(service)) {
            qDebug() << "D-Bus service registered successfully";
            registerDBusObject(&window);
            registerDBusObject(window.ocr());
        } else {
            qWarning() << QCoreApplication::translate("D-Bus", "D-Bus service %1 is already registered by another application").arg(service);
            qDebug() << "D-Bus connection error:" << QDBusConnection::sessionBus().lastError().message();
        }
    } else {
        qWarning() << "D-Bus session bus is not connected";
    }
#endif

    return QCoreApplication::exec();
}

int launchCli(int argc, char *argv[])
{
    Q_INIT_RESOURCE(engines);
    Q_INIT_RESOURCE(icon_theme);

    const QCoreApplication app(argc, argv);

    AppSettings settings;
    settings.loadCustomLanguageRegistry(); // Load persisted custom languages
    Language::setCustomLanguageRegistryChangedCallback(&AppSettings::onCustomLanguageRegistryChanged);
    settings.setupLocalization();

    Cli cli;
    // Deferred rather than called directly: process() can run all the way to
    // completion before exec() starts - --codes prints and returns, and a
    // provider that answers synchronously (Copy) finishes inside the call.
    // QCoreApplication::quit() issued with no event loop running is simply
    // discarded, so the process then sat forever with its work already done.
    QTimer::singleShot(0, &cli, [&cli, &app] {
        cli.process(app);
    });

    return QCoreApplication::exec();
}

#ifdef Q_OS_UNIX
void registerDBusObject(QObject *object)
{
    const QString objectPath = QStringLiteral("/%1/").arg(QStringLiteral(APPLICATION_ID).replace('.', '/'));
    const QString fullPath = objectPath + object->metaObject()->className();
    qDebug() << "Registering D-Bus object:" << object->metaObject()->className() << "at path:" << fullPath;
    if (!QDBusConnection::sessionBus().registerObject(fullPath, object, QDBusConnection::ExportScriptableSlots)) {
        qWarning() << QCoreApplication::translate("D-Bus", "Unable to register D-Bus object for %1").arg(object->metaObject()->className());
        qDebug() << "D-Bus object registration error:" << QDBusConnection::sessionBus().lastError().message();
    } else {
        qDebug() << "D-Bus object registered successfully:" << object->metaObject()->className();
    }
}
#endif
