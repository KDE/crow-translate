/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cmake.h"
#include "frontend/frontendregistry.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationVersion(QStringLiteral("%1.%2.%3").arg(VERSION_MAJOR).arg(VERSION_MINOR).arg(VERSION_PATCH));
    QCoreApplication::setApplicationName(QStringLiteral(PROJECT_NAME));
    QCoreApplication::setOrganizationName(QStringLiteral(PROJECT_NAME));

    // Built by hand rather than taken from QCoreApplication::arguments(),
    // which needs an application object - and which one to build is exactly
    // what is being decided here.
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    const QString frontendId = FrontendRegistry::resolveId(arguments);
    std::unique_ptr<ICrowFrontend> frontend = FrontendRegistry::create(frontendId);

    if (frontend == nullptr) {
        QTextStream stderrStream(stderr);
        stderrStream << QCoreApplication::translate("main", "Error: Unknown frontend '%1'").arg(frontendId) << Qt::endl;

        QStringList names;
        const QList<FrontendInfo> frontends = FrontendRegistry::available();
        names.reserve(frontends.size());
        for (const FrontendInfo &info : frontends)
            names.append(QStringLiteral("%1 (%2)").arg(info.id, info.displayName));

        stderrStream << QCoreApplication::translate("main", "Available frontends: %1").arg(names.join(QStringLiteral(", "))) << Qt::endl;
        return 1;
    }

    // Before the application object exists, because some of it has to be:
    // KIconTheme::initTheme() installs a startup hook, Windows DPI awareness
    // has to be declared before the first window, and resources compiled into
    // a static library need registering.
    frontend->prepareEnvironment();

    const std::unique_ptr<QCoreApplication> app = frontend->createApplication(argc, argv);
    return frontend->run(*app);
}
