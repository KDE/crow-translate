/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "hellottsplugin.h"

#include "hellottsprovider.h"

QVector<ProviderOptionSpec> HelloTtsPlugin::options() const
{
    return {
        // A directory, which is the one option shape worth declaring rather
        // than drawing: the settings dialog renders it as a line edit with a
        // browse button, and every backend with a path used to build those
        // sixty lines of widget for itself.
        makeDirectoryOptionSpec(QStringLiteral("outputDirectory"),
                                QString(),
                                tr("Output folder"),
                                tr("Where to write the text this backend was asked to speak.")),
    };
}

ATTSProvider *HelloTtsPlugin::createProvider(QObject *parent)
{
    return new HelloTtsProvider(parent);
}
