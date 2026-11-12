/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "xdgdesktopportal.h"

#include <QGuiApplication>
#include <QWindow>

#ifdef WITH_KWINDOWSYSTEM
#include <QEventLoop>
#include <QTimer>

#include <KWaylandExtras>
#include <KWindowSystem>
#include <chrono>
#include <kwindowsystem_version.h>

// KWindowSystem answers through a future since 6.28, and through a signal
// before that - which it has deprecated since.
#if KWINDOWSYSTEM_VERSION >= QT_VERSION_CHECK(6, 28, 0)
#define CROW_EXPORT_ANSWERS_WITH_A_FUTURE
#include <QFuture>
#include <QFutureWatcher>
#endif

using namespace std::chrono_literals;

namespace
{
// The handle the compositor gives out for the window once it is exported
// (xdg-foreign), or nothing if it gives none in time. One that does not
// answer - it need not know the protocol - must not leave the caller
// waiting: a portal request without a parent is still a request. The wait is
// counted from where the question has been asked, because asking it can
// take a while the first time, when KWindowSystem loads its Wayland side.
QString exportedHandle(QWindow *window)
{
    const auto patience = 1s;
    QEventLoop loop;

#ifdef CROW_EXPORT_ANSWERS_WITH_A_FUTURE
    const QFuture<QString> exported = KWaylandExtras::exportToplevel(window);
    if (!exported.isFinished()) {
        QFutureWatcher<QString> watcher;
        QObject::connect(&watcher, &QFutureWatcher<QString>::finished, &loop, &QEventLoop::quit);
        watcher.setFuture(exported);
        QTimer::singleShot(patience, &loop, &QEventLoop::quit);
        loop.exec();
    }
    if (exported.isFinished() && exported.isResultReadyAt(0)) {
        return exported.result();
    }
    return {};
#else
    QString handle;
    bool answered = false;
    QObject::connect(KWaylandExtras::self(), &KWaylandExtras::windowExported, &loop, [&handle, &answered, &loop, window](const QWindow *exported, const QString &name) {
        if (exported == window) {
            handle = name;
            answered = true;
            loop.quit();
        }
    });
    KWaylandExtras::exportWindow(window);
    if (!answered) {
        QTimer::singleShot(patience, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return handle;
#endif
}
}
#endif

QString XdgDesktopPortal::parentWindow(QWindow *activeWindow)
{
    if (activeWindow == nullptr) {
        return {};
    }

    if (qGuiApp->nativeInterface<QNativeInterface::QX11Application>() != nullptr) {
        return QStringLiteral("x11:%1").arg(activeWindow->winId(), 0, 16);
    }

#ifdef WITH_KWINDOWSYSTEM
    // On Wayland a window has no id another process could name it by, only
    // the handle of its export. KWindowSystem keeps the window exported for
    // as long as it lives, so the handle is still good when the portal gets
    // round to using it, and asking again gives the same one.
    if (KWindowSystem::isPlatformWayland()) {
        const QString handle = exportedHandle(activeWindow);
        if (!handle.isEmpty()) {
            return QStringLiteral("wayland:%1").arg(handle);
        }
    }
#endif

    return {};
}
