/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "frontend/clifrontend.h"

#include "cli.h"
#include "language.h"
#include "settings/appsettings.h"

#include <QCoreApplication>
#include <QTimer>

CliFrontend::CliFrontend() = default;

CliFrontend::~CliFrontend() = default;

void CliFrontend::prepareEnvironment()
{
    Q_INIT_RESOURCE(engines);
    Q_INIT_RESOURCE(icon_theme);
}

std::unique_ptr<QCoreApplication> CliFrontend::createApplication(int &argc, char **argv)
{
    return std::make_unique<QCoreApplication>(argc, argv);
}

int CliFrontend::run(QCoreApplication &app)
{
    AppSettings settings;
    settings.loadCustomLanguageRegistry(); // Load persisted custom languages
    Language::setCustomLanguageRegistryChangedCallback(&AppSettings::onCustomLanguageRegistryChanged);
    settings.setupLocalization();

    m_cli = std::make_unique<Cli>();
    // Deferred rather than called directly: process() can run all the way to
    // completion before exec() starts - --codes prints and returns, and a
    // provider that answers synchronously (Copy) finishes inside the call.
    // QCoreApplication::quit() issued with no event loop running is simply
    // discarded, so the process then sat forever with its work already done.
    QTimer::singleShot(0, m_cli.get(), [this, &app] {
        m_cli->process(app);
    });

    return QCoreApplication::exec();
}
