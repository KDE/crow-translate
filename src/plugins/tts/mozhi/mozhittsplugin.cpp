/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mozhittsplugin.h"

#include "mozhittsprovider.h"
#include "onlinetranslator.h"

QVector<ProviderOptionSpec> MozhiTtsPlugin::options() const
{
    // The same two keys the translation backend of the same name uses, and
    // deliberately so: one Mozhi instance serves both, and configuring it
    // twice would be a way to have it configured inconsistently.
    // Drawn by hand, like the translation backend of the same name: they are
    // the same two controls on the same page.
    auto handDrawn = [](ProviderOptionSpec spec) {
        spec.userVisible = false;
        return spec;
    };
    return {
        handDrawn(makeOptionSpec(QStringLiteral("instance"),
                                 QStringLiteral("Translation/Instance"),
                                 QMetaType::QString,
                                 QStringLiteral("https://mozhi.aryak.me"),
                                 tr("Instance URL"))),
        handDrawn(makeOptionSpec(QStringLiteral("engine"),
                                 QStringLiteral("MainWindow/CurrentEngine"),
                                 QMetaType::Int,
                                 static_cast<int>(OnlineTranslator::Google),
                                 tr("Engine"))),
    };
}

ATTSProvider *MozhiTtsPlugin::createProvider(QObject *parent)
{
    return new MozhiTTSProvider(parent);
}
