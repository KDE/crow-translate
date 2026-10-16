/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TESTDISPLAY_H
#define TESTDISPLAY_H

#include <QtGlobal>

// Whether a GUI test has somewhere to put a window. On X11 and Wayland that is
// the display the session advertises; Windows and macOS always have one. The
// tests used to look for DISPLAY or WAYLAND_DISPLAY everywhere, so on Windows
// every GUI test skipped itself and the suite passed without running them.
inline bool hasDisplay()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    return !qEnvironmentVariableIsEmpty("DISPLAY") || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
#else
    return true;
#endif
}

#endif // TESTDISPLAY_H
