/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CLIFRONTEND_H
#define CLIFRONTEND_H

#include "frontend/icrowfrontend.h"

#include <memory>

class Cli;

// The command line. A QCoreApplication is enough - no GUI connection is
// made, which is what lets it run over ssh and in a build container.
class CliFrontend : public ICrowFrontend
{
public:
    CliFrontend();
    ~CliFrontend() override;

    void prepareEnvironment() override;
    std::unique_ptr<QCoreApplication> createApplication(int &argc, char **argv) override;
    int run(QCoreApplication &app) override;

private:
    std::unique_ptr<Cli> m_cli;
};

#endif // CLIFRONTEND_H
