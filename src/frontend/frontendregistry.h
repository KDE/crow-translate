/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FRONTENDREGISTRY_H
#define FRONTENDREGISTRY_H

#include "frontend/icrowfrontend.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

// Which frontends exist, and which one an invocation asked for.
//
// Statically linked and listed in one place. A QPluginLoader-backed
// implementation could replace the body of available()/create() without any
// frontend or caller changing, which is the point of keeping the lookup
// behind a registry rather than writing an if-else in main().
class FrontendRegistry
{
public:
    static QList<FrontendInfo> available();
    static std::optional<FrontendInfo> find(const QString &id);
    static std::unique_ptr<ICrowFrontend> create(const QString &id);

    // Which frontend an argument list asks for.
    //
    // Parsed by hand rather than with QCommandLineParser, and it has to be:
    // the answer decides which QCoreApplication subclass to build, and
    // QCommandLineParser needs one to already exist. Only --frontend is
    // looked for; everything else is left untouched for the chosen frontend
    // to parse properly.
    //
    // With no arguments at all this is the window, which is what running the
    // application from a launcher does. Otherwise the command line, which is
    // what the old argc == 1 test in main() meant.
    static QString resolveId(const QStringList &arguments);

    // The option resolveId() looks for, so a frontend can declare it in its
    // own parser and have it show up in --help instead of being rejected as
    // unknown.
    static QString frontendOptionName();
};

#endif // FRONTENDREGISTRY_H
