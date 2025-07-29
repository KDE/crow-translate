/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "waylandplasmascreengrabber.h"

#include <QCursor>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QWindow>
#include <QtConcurrent>

#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>

using ScreenImagesMap = QMap<const QScreen *, QImage>;

WaylandPlasmaScreenGrabber::WaylandPlasmaScreenGrabber(QObject *parent)
    : DBusScreenGrabber(parent)
    , m_interface(QStringLiteral("org.kde.KWin.ScreenShot2"),
                  QStringLiteral("/org/kde/KWin/ScreenShot2"),
                  QStringLiteral("org.kde.KWin.ScreenShot2"))
{
    const static int id = qRegisterMetaType<ScreenImagesMap>("ScreenImagesMap");
    Q_UNUSED(id)

    // Get interface version on construction
    m_version = getInterfaceVersion();
    qDebug() << "WaylandPlasmaScreenGrabber: Detected KWin ScreenShot2 interface version:" << m_version;
}

bool WaylandPlasmaScreenGrabber::isAvailable()
{
    const QDBusInterface interface(QStringLiteral("org.kde.KWin.ScreenShot2"),
                                   QStringLiteral("/org/kde/KWin/ScreenShot2"),
                                   QStringLiteral("org.kde.KWin.ScreenShot2"));
    return interface.isValid();
}

void WaylandPlasmaScreenGrabber::grab()
{
    QVarLengthArray<int, 2> pipe{0, 0};
    if (pipe2(pipe.data(), O_CLOEXEC | O_NONBLOCK) != 0) {
        std::array<char, 80> tmp_buf{};
        showError(tr("Unable to create pipe: %1.").arg(strerror_r(errno, tmp_buf.data(), tmp_buf.size())));
        return;
    }

    QVariantMap options;
    options["include-cursor"] = false;
    options["native-resolution"] = true;

    QDBusPendingReply<QVariantMap> reply;

    if (m_version >= 4) {
        // Version 4+: Use CaptureActiveScreen, extracts screen info from result
        qDebug() << "WaylandPlasmaScreenGrabber: Using CaptureActiveScreen (version 4+)";
        reply = m_interface.asyncCall(QStringLiteral("CaptureActiveScreen"),
                                      QVariant::fromValue(options),
                                      QVariant::fromValue(QDBusUnixFileDescriptor(pipe[1])));
    } else if (m_version >= 3) {
        // Version 3: Use CaptureWorkspace, detect screen from geometry/cursor
        qDebug() << "WaylandPlasmaScreenGrabber: Using CaptureWorkspace (version 3)";
        reply = m_interface.asyncCall(QStringLiteral("CaptureWorkspace"),
                                      QVariant::fromValue(options),
                                      QVariant::fromValue(QDBusUnixFileDescriptor(pipe[1])));
    } else {
        // Version 2 fallback: Use CaptureScreen with detected screen
        qDebug() << "WaylandPlasmaScreenGrabber: Using CaptureScreen fallback (version 2)";
        auto *screen = QGuiApplication::screenAt(QCursor::pos());
        if (screen == nullptr) {
            close(pipe[0]);
            close(pipe[1]);
            showError(tr("Unable to detect active screen for screenshot"));
            return;
        }
        m_screen = screen;
        reply = m_interface.asyncCall(QStringLiteral("CaptureScreen"),
                                      QVariant::fromValue(m_screen->name()),
                                      QVariant::fromValue(options),
                                      QVariant::fromValue(QDBusUnixFileDescriptor(pipe[1])));
    }
    m_callWatcher = new QDBusPendingCallWatcher(reply, this);

    connect(m_callWatcher, &QDBusPendingCallWatcher::finished, [this, socketDescriptor = pipe[0]] {
        const QDBusPendingReply<QVariantMap> reply = readReply<void>();

        if (!reply.isValid()) {
            const QDBusError error = reply.error();
            const QString errorMsg = error.message();
            qDebug() << "WaylandPlasmaScreenGrabber: D-Bus call failed:" << errorMsg;
            qDebug() << "WaylandPlasmaScreenGrabber: D-Bus error type:" << error.type();
            qDebug() << "WaylandPlasmaScreenGrabber: D-Bus error name:" << error.name();

            // Check for permission/authorization errors using D-Bus error type
            if (error.name() == "org.kde.KWin.ScreenShot2.Error.NoAuthorized") {
                showPermissionWarning();
                fallbackToCaptureInteractive();
                return;
            }

            showError(errorMsg);
            return;
        }

        const QVariantMap result = reply.value();
        auto format = result["format"].value<QImage::Format>();

        // Store screen info for version 4+ which provides screen name in result
        if (m_version >= 4 && result.contains("screen")) {
            const QString screenName = result["screen"].toString();
            qDebug() << "WaylandPlasmaScreenGrabber: CaptureActiveScreen returned screen:" << screenName;
            // Find screen by name
            const auto screens = QGuiApplication::screens();
            for (auto *screen : screens) {
                if (screen->name() == screenName) {
                    m_screen = screen;
                    break;
                }
            }
        }

        m_readImageFuture = QtConcurrent::run([this, socketDescriptor, format] {
            readPixmapFromSocket(socketDescriptor, format);
            close(socketDescriptor);
        });
    });
    close(pipe[1]);
}

void WaylandPlasmaScreenGrabber::cancel()
{
    DBusScreenGrabber::cancel();

    m_readImageFuture.cancel();
}

void WaylandPlasmaScreenGrabber::readPixmapFromSocket(int socketDescriptor, QImage::Format format)
{
    QByteArray data;
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(socketDescriptor, &readset);
    timeval timeout{30, 0};
    QVarLengthArray<char, 4096 * 16> buffer;

    forever {
        if (m_readImageFuture.isCanceled())
            return;

        const int ready = select(FD_SETSIZE, &readset, nullptr, nullptr, &timeout);
        if (ready < 0) {
            std::array<char, 80> tmp_buf{};
            QMetaObject::invokeMethod(this, "showError", Q_ARG(QString, tr("Unable to wait for socket readiness: %1.").arg(strerror_r(errno, tmp_buf.data(), tmp_buf.size()))));
            return;
        }

        if (ready == 0) {
            QMetaObject::invokeMethod(this, "showError", Q_ARG(QString, tr("Timeout reading from pipe.")));
            return;
        }

        const ssize_t bytesRead = read(socketDescriptor, buffer.data(), buffer.capacity());
        if (bytesRead < 0) {
            std::array<char, 80> tmp_buf{};
            QMetaObject::invokeMethod(this, "showError", Q_ARG(QString, tr("Unable to read data from socket: %1.").arg(strerror_r(errno, tmp_buf.data(), tmp_buf.size()))));
            return;
        }
        if (bytesRead == 0) {
            QDataStream lDataStream(data);

            if (m_version >= 3 && (m_screen == nullptr)) {
                // For version 3+ CaptureWorkspace, we need to detect the screen from the full workspace image
                // We need to determine the workspace dimensions from all screens
                QRect workspaceRect;
                const auto screens = QGuiApplication::screens();
                for (auto *screen : screens) {
                    workspaceRect = workspaceRect.united(screen->geometry());
                }

                // Create the workspace image with the combined dimensions
                QImage workspaceImage{workspaceRect.size(), format};
                lDataStream.readRawData(reinterpret_cast<char *>(workspaceImage.bits()), workspaceImage.sizeInBytes());

                // Detect which screen the cursor is on and extract that portion
                detectScreenFromWorkspace(workspaceImage, QCursor::pos());
                return;
            }

            // For version 4+ (CaptureActiveScreen) or version 2 (CaptureScreen), we already have the correct screen
            if (m_screen == nullptr) {
                // Fallback: use primary screen if we somehow don't have one set
                m_screen = QGuiApplication::primaryScreen();
            }

            auto geo = m_screen->geometry();
            auto resultImage = QImage{geo.width(), geo.height(), format};
            lDataStream.readRawData(reinterpret_cast<char *>(resultImage.bits()), resultImage.sizeInBytes());
            const QMetaMethod grabbed = QMetaMethod::fromSignal(&WaylandPlasmaScreenGrabber::grabbed);
            grabbed.invoke(this, ScreenImagesMap{{m_screen, resultImage}});
            m_screen = nullptr;
            return;
        }

        data.append(buffer.data(), static_cast<int>(bytesRead));
    }
}

uint WaylandPlasmaScreenGrabber::getInterfaceVersion()
{
    QDBusInterface interface(QStringLiteral("org.kde.KWin.ScreenShot2"),
                             QStringLiteral("/org/kde/KWin/ScreenShot2"),
                             QStringLiteral("org.kde.KWin.ScreenShot2"));

    if (!interface.isValid()) {
        qDebug() << "WaylandPlasmaScreenGrabber: Interface not valid, assuming version 2";
        return 2;
    }

    // Try to get version property
    const QVariant versionVariant = interface.property("Version");
    if (versionVariant.isValid()) {
        bool ok = false;
        const uint version = versionVariant.toUInt(&ok);
        if (ok && version > 0) {
            return version;
        }
    }

    // Fallback: try to detect based on available methods
    const QDBusReply<QStringList> introspectReply = interface.call("Introspect");
    if (introspectReply.isValid()) {
        const QStringList methods = introspectReply.value();
        if (methods.contains("CaptureWorkspace")) {
            return 3; // Has CaptureWorkspace, so at least version 3
        }
    }

    // Default to version 2 if we can't determine
    qDebug() << "WaylandPlasmaScreenGrabber: Unable to determine version, assuming version 2";
    return 2;
}

void WaylandPlasmaScreenGrabber::detectScreenFromWorkspace(const QImage &workspaceImage, const QPoint &cursorPos)
{
    qDebug() << "WaylandPlasmaScreenGrabber: Detecting screen from workspace, cursor at:" << cursorPos;

    // Find which screen contains the cursor position
    const auto screens = QGuiApplication::screens();
    QScreen *targetScreen = nullptr;

    for (auto *screen : screens) {
        if (screen->geometry().contains(cursorPos)) {
            targetScreen = screen;
            break;
        }
    }

    if (targetScreen == nullptr) {
        qDebug() << "WaylandPlasmaScreenGrabber: No screen contains cursor, using primary screen";
        targetScreen = QGuiApplication::primaryScreen();
    }

    if (targetScreen == nullptr) {
        QMetaObject::invokeMethod(this, "showError", Q_ARG(QString, tr("Unable to determine target screen")));
        return;
    }

    // Calculate workspace offset (in case screens have negative coordinates)
    QRect workspaceRect;
    for (auto *screen : screens) {
        workspaceRect = workspaceRect.united(screen->geometry());
    }

    // Calculate the area of the workspace image that corresponds to this screen
    const QRect screenGeometry = targetScreen->geometry();
    const QRect extractRect = screenGeometry.translated(-workspaceRect.topLeft());

    qDebug() << "WaylandPlasmaScreenGrabber: Target screen geometry:" << screenGeometry;
    qDebug() << "WaylandPlasmaScreenGrabber: Workspace rect:" << workspaceRect;
    qDebug() << "WaylandPlasmaScreenGrabber: Extract rect:" << extractRect;

    // Extract the screen portion from the workspace image
    QImage screenImage = workspaceImage.copy(extractRect);

    if (screenImage.isNull()) {
        QMetaObject::invokeMethod(this, "showError", Q_ARG(QString, tr("Failed to extract screen area from workspace")));
        return;
    }

    m_screen = targetScreen;
    const QMetaMethod grabbed = QMetaMethod::fromSignal(&WaylandPlasmaScreenGrabber::grabbed);
    grabbed.invoke(this, ScreenImagesMap{{m_screen, screenImage}});
    m_screen = nullptr;
}

void WaylandPlasmaScreenGrabber::showPermissionWarning()
{
    // Show a system tray notification about missing permissions
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        QSystemTrayIcon trayIcon;
        trayIcon.setIcon(QIcon::fromTheme("dialog-warning"));
        trayIcon.show();
        trayIcon.showMessage(
            tr("Screenshot Permission Required"),
            tr("Crow Translate needs permission to take screenshots. Please grant permission in your system settings or use the interactive screenshot mode."),
            QSystemTrayIcon::Warning,
            5000 // 5 seconds
        );
    } else {
        // Fallback to error dialog if no system tray
        QMetaObject::invokeMethod(this, "showError", Q_ARG(QString, tr("Screenshot permission denied. Please grant permission in system settings or try interactive mode.")));
    }
}

void WaylandPlasmaScreenGrabber::fallbackToCaptureInteractive()
{
    qDebug() << "WaylandPlasmaScreenGrabber: Falling back to CaptureInteractive";

    QVarLengthArray<int, 2> pipe{0, 0};
    if (pipe2(pipe.data(), O_CLOEXEC | O_NONBLOCK) != 0) {
        std::array<char, 80> tmp_buf{};
        showError(tr("Unable to create pipe: %1.").arg(strerror_r(errno, tmp_buf.data(), tmp_buf.size())));
        return;
    }

    QVariantMap options;
    options["include-cursor"] = false;
    options["include-decoration"] = true;
    options["include-shadow"] = true;
    options["native-resolution"] = true;

    // Use CaptureInteractive with kind=1 (screen)
    const QDBusPendingReply<QVariantMap> reply = m_interface.asyncCall(
        QStringLiteral("CaptureInteractive"),
        static_cast<uint>(1), // kind: 1 = screen
        QVariant::fromValue(options),
        QVariant::fromValue(QDBusUnixFileDescriptor(pipe[1])));

    m_callWatcher = new QDBusPendingCallWatcher(reply, this);
    connect(m_callWatcher, &QDBusPendingCallWatcher::finished, [this, socketDescriptor = pipe[0]] {
        const QDBusPendingReply<QVariantMap> reply = readReply<void>();

        if (!reply.isValid()) {
            showError(reply.error().message());
            return;
        }

        const QVariantMap result = reply.value();
        auto format = result["format"].value<QImage::Format>();

        // For interactive capture, we might not know the exact screen, so use primary
        m_screen = QGuiApplication::primaryScreen();

        m_readImageFuture = QtConcurrent::run([this, socketDescriptor, format] {
            readPixmapFromSocket(socketDescriptor, format);
            close(socketDescriptor);
        });
    });
    close(pipe[1]);
}
