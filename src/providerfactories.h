/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PROVIDERFACTORIES_H
#define PROVIDERFACTORIES_H

#include "tts/attsprovider.h"

// What is left of the switch-over-every-backend factories.
//
// The translation half is gone: PluginManager asks a plugin to build its own
// provider and so never needs the list. The speech half follows in the next
// step of #738; until then it lives here, in the application, because a
// factory that names every concrete provider cannot sit in the base class
// those providers derive from.
namespace ProviderFactories
{

ATTSProvider *createTTSProvider(QObject *parent, ATTSProvider::ProviderBackend chosenBackend);

} // namespace ProviderFactories

#endif // PROVIDERFACTORIES_H
