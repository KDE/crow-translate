/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "foreignplugin.h"

#include "translator/copytranslationprovider.h"

QVector<ProviderOptionSpec> ForeignTranslationPlugin::options() const
{
    // None, and CopyTranslationProvider takes none - which is the agreement
    // testDeclaredOptionsAreTheOptionsTheProviderTakes() checks for.
    return {};
}

ATranslationProvider *ForeignTranslationPlugin::createProvider(QObject *parent)
{
    return new CopyTranslationProvider(parent);
}
