/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PROVIDERFACTORIES_H
#define PROVIDERFACTORIES_H

#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

// Building a provider from a backend choice, and recovering when that fails.
//
// These were static members of the two abstract bases. The bases have moved
// into crow-core, which providers link and the application links; a factory
// that names every concrete provider cannot go with them without the core
// depending on everything built on top of it.
//
// Free functions in the application for now. #738 replaces them with
// PluginManager, which asks a plugin to build its own provider and so never
// needs to know the list at all.
namespace ProviderFactories
{

ATranslationProvider *createTranslationProvider(QObject *parent, ATranslationProvider::ProviderBackend chosenBackend);
ATTSProvider *createTTSProvider(QObject *parent, ATTSProvider::ProviderBackend chosenBackend);

} // namespace ProviderFactories

#endif // PROVIDERFACTORIES_H
