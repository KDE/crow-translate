/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qtttsplugin.h"

#include "qtttsprovider.h"

QVector<ProviderOptionSpec> QtTtsPlugin::options() const
{
    // Nothing of its own: the voice and the rate are chosen through the
    // window's controls, which every TTS backend answers the same way.
    return {};
}

ATTSProvider *QtTtsPlugin::createProvider(QObject *parent)
{
    return new QtTTSProvider(parent);
}
