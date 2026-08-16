/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

#ifdef WITH_KICONTHEMES
#include <KIconLoader>
#endif

namespace IconUtils
{
/*
 * Icon loading helpers with theme fallback.
 *
 * Some icon names crow-translate uses (e.g. "edit-clear-all") are breeze
 * names that older themes like Oxygen do not provide (bug 509329). When
 * KIconThemes is available we resolve through KIconLoader, which walks the
 * full KDE theme chain (configured theme -> its Inherits -> fallback theme
 * -> the themes compiled into the binary via qrc). KIconLoader requires a
 * living QGuiApplication, so only call these after the app object exists.
 *
 * canReturnNull=true makes KIconLoader return a null pixmap instead of the
 * "unknown" placeholder when nothing is found, so the QIcon::fromTheme
 * fallback can engage.
 */
inline QIcon load(const QString &name)
{
#ifdef WITH_KICONTHEMES
    const QPixmap pixmap = KIconLoader::global()->loadIcon(name, KIconLoader::Panel, 22, KIconLoader::DefaultState, QStringList(), nullptr, true);
    if (!pixmap.isNull()) {
        return QIcon(pixmap);
    }
#endif
    return QIcon::fromTheme(name);
}

inline QIcon loadDesktop(const QString &name)
{
#ifdef WITH_KICONTHEMES
    const QPixmap pixmap = KIconLoader::global()->loadIcon(name, KIconLoader::Desktop, 512, KIconLoader::DefaultState, QStringList(), nullptr, true);
    if (!pixmap.isNull()) {
        return QIcon(pixmap);
    }
#endif
    return QIcon::fromTheme(name);
}
} // namespace IconUtils
