/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mozhiplugin.h"

#include "mozhitranslationprovider.h"
#include "onlinetranslator.h"

QVector<ProviderOptionSpec> MozhiTranslationPlugin::options() const
{
    // Every settings key here is the one 4.1.0 already writes. That is the
    // point of the schema carrying the key rather than deriving one from the
    // plugin id: nobody's configuration moves.
    return {
        makeOptionSpec(QStringLiteral("instance"),
                       QStringLiteral("Translation/Instance"),
                       QMetaType::QString,
                       QStringLiteral("https://mozhi.aryak.me"),
                       tr("Instance URL"),
                       tr("The Mozhi instance to send translations through.")),
        // Stored as the OnlineTranslator::Engine enum rather than a plain
        // int - QSettings wrote it that way long before this - but the value
        // handed to the provider is the ordinal, which a QVariant holding the
        // enum converts to without help.
        makeOptionSpec(QStringLiteral("engine"),
                       QStringLiteral("MainWindow/CurrentEngine"),
                       QMetaType::Int,
                       static_cast<int>(OnlineTranslator::Google),
                       tr("Engine"),
                       tr("Which upstream translator the instance should use.")),
        makeOptionSpec(QStringLiteral("apikey"),
                       QStringLiteral("Translation/LibreTranslateApiKey"),
                       QMetaType::QString,
                       QString(),
                       tr("LibreTranslate API key")),
        makeOptionSpec(QStringLiteral("direct"),
                       QStringLiteral("Translation/LibreTranslateDirect"),
                       QMetaType::Bool,
                       false,
                       tr("Contact LibreTranslate directly")),
    };
}

ATranslationProvider *MozhiTranslationPlugin::createProvider(QObject *parent)
{
    return new MozhiTranslationProvider(parent);
}
