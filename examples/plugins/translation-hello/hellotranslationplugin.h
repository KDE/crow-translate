/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HELLOTRANSLATIONPLUGIN_H
#define HELLOTRANSLATIONPLUGIN_H

#include "plugininterfaces/itranslationplugin.h"

#include <QObject>

// The plugin half: how the application gets a backend, and what it may be
// configured with.
//
// Three things make this loadable, and all three have to be right or the
// module is skipped without a word:
//
//   Q_OBJECT         - moc has to see this class. Without AUTOMOC (or a
//                      qt_wrap_cpp) the module builds and declares no plugin.
//   Q_PLUGIN_METADATA- the IID is the version gate. It comes from the header
//                      rather than being typed out, so a plugin built against
//                      a later interface stops being loaded rather than being
//                      loaded and found short a vtable entry. FILE names the
//                      JSON beside this header, which the host reads *without*
//                      loading the module.
//   Q_INTERFACES     - what qobject_cast<ITranslationPlugin*> matches on.
//                      Omitting it gives a module that loads and then answers
//                      "not a translation plugin".
class HelloTranslationPlugin : public QObject
    , public ITranslationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CROW_TRANSLATION_PLUGIN_IID FILE "hello.json")
    Q_INTERFACES(ITranslationPlugin)

public:
    QVector<ProviderOptionSpec> options() const override;
    ATranslationProvider *createProvider(QObject *parent) override;
};

#endif // HELLOTRANSLATIONPLUGIN_H
