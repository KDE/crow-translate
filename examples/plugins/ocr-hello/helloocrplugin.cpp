/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "helloocrplugin.h"

#include "helloocrprovider.h"

QVector<ProviderOptionSpec> HelloOcrPlugin::options() const
{
    return {
        makeOptionSpec(QStringLiteral("label"),
                       QString(),
                       QMetaType::QString,
                       QStringLiteral("hello"),
                       tr("Label"),
                       tr("Put in front of what this engine reports.")),
    };
}

AOcrProvider *HelloOcrPlugin::createProvider(QObject *parent)
{
    return new HelloOcrProvider(parent);
}

// The one thing recognition plugins carry that the other two families do not:
// the settings dialog offers a list of languages to recognize in, and only the
// engine knows what data it has. An engine not configured by language - a
// vision model is not - returns nothing, which is the inherited default and
// need not be overridden at all. It is written out here to say so.
QStringList HelloOcrPlugin::availableLanguages(const QString &dataPath) const
{
    Q_UNUSED(dataPath)
    return {};
}
