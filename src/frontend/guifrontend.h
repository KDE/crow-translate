/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef GUIFRONTEND_H
#define GUIFRONTEND_H

#include "frontend/icrowfrontend.h"

#include <memory>

class MainWindow;

// The window. Everything launchGui() used to do, with the parts that must
// happen before an application object exists separated out - see
// ICrowFrontend for why that split is not optional.
class GuiFrontend : public ICrowFrontend
{
public:
    GuiFrontend();
    ~GuiFrontend() override;

    void prepareEnvironment() override;
    std::unique_ptr<QCoreApplication> createApplication(int &argc, char **argv) override;
    int run(QCoreApplication &app) override;

private:
    std::unique_ptr<MainWindow> m_window;
};

#endif // GUIFRONTEND_H
