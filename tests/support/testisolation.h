/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TESTISOLATION_H
#define TESTISOLATION_H

#include "settings/appsettings.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

// Every test binary that touches AppSettings (directly, or indirectly by
// constructing a MainWindow/SettingsDialog) must not read or write the
// real ~/.config/crow-translate-tests/ file - sharing it across binaries
// and ctest invocations means each run's result silently depends on
// whatever state a previous, unrelated run happened to leave behind
// (found the hard way: a stale ShowPrivacyPopup=false left one test
// binary's modal-dialog-on-first-run bug invisible for an entire session
// of ctest runs, because an earlier binary had already answered it).
//
// That cuts both ways: a genuinely fresh settings dir has *no* persisted
// ShowPrivacyPopup=false either, so MainWindow's loadMainWindowSettings()
// pops a real blocking QMessageBox on its very first construction in every
// test that doesn't answer it first. Answering it here, once, is the
// actual root-cause fix - not just isolating *which* file gets that
// leftover answer.
//
// Call this FIRST in main(), before constructing SingleApplication/
// QCoreApplication - it points XDG_CONFIG_HOME at a fresh QTemporaryDir
// that outlives the process (leaked deliberately; short-lived test
// process, OS reclaims it) and pins the org/app name used everywhere else
// in this test suite.
inline void isolateTestSettings()
{
    static QTemporaryDir *configDir = new QTemporaryDir();
    qputenv("XDG_CONFIG_HOME", configDir->path().toUtf8());

    QCoreApplication::setOrganizationName(QStringLiteral("crow-translate-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("crow-translate-tests"));

    AppSettings settings;
    settings.setShowPrivacyPopup(false);
}

#endif // TESTISOLATION_H
