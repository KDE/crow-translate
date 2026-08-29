/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "frontend/frontendregistry.h"

#include "frontend/clifrontend.h"
#include "frontend/guifrontend.h"

#include <QCoreApplication>

namespace
{
const QString kGuiId = QStringLiteral("gui");
const QString kCliId = QStringLiteral("cli");
const QString kOptionName = QStringLiteral("frontend");
} // namespace

QList<FrontendInfo> FrontendRegistry::available()
{
    return {
        {kGuiId, QCoreApplication::translate("FrontendRegistry", "Graphical window")},
        {kCliId, QCoreApplication::translate("FrontendRegistry", "Command line")},
    };
}

std::optional<FrontendInfo> FrontendRegistry::find(const QString &id)
{
    const QList<FrontendInfo> frontends = available();
    for (const FrontendInfo &info : frontends) {
        if (info.id == id)
            return info;
    }
    return std::nullopt;
}

std::unique_ptr<ICrowFrontend> FrontendRegistry::create(const QString &id)
{
    if (id == kGuiId)
        return std::make_unique<GuiFrontend>();
    if (id == kCliId)
        return std::make_unique<CliFrontend>();
    return nullptr;
}

QString FrontendRegistry::resolveId(const QStringList &arguments)
{
    // arguments includes argv[0].
    for (int i = 1; i < arguments.size(); ++i) {
        const QString &argument = arguments.at(i);

        if (argument == QLatin1String("--") + kOptionName) {
            if (i + 1 < arguments.size())
                return arguments.at(i + 1);
            // Malformed. Hand it to the command line, whose parser will
            // produce a proper "requires a value" message.
            return kCliId;
        }

        const QString prefix = QLatin1String("--") + kOptionName + QLatin1Char('=');
        if (argument.startsWith(prefix))
            return argument.mid(prefix.size());
    }

    return arguments.size() <= 1 ? kGuiId : kCliId;
}

QString FrontendRegistry::frontendOptionName()
{
    return kOptionName;
}
