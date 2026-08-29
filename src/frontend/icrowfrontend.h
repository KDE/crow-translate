/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ICROWFRONTEND_H
#define ICROWFRONTEND_H

#include <QString>

#include <memory>

class QCoreApplication;

// What every way of presenting Crow Translate has in common.
//
// main() used to decide by counting arguments - no arguments meant the
// window, anything else meant the command line - and then run one of two
// hand-written launch functions. That had two consequences worth naming: the
// window could never be given an option, because passing one sent you to the
// CLI instead, and adding a third way to present the application (a compact
// window, a large-print one) meant a third launch function and another branch
// in main().
//
// The three calls below are in the order main() makes them, and the split
// between them is not arbitrary. Some setup has to happen before any
// QCoreApplication exists at all - KIconTheme::initTheme() installs a startup
// hook, Windows DPI awareness must be declared before the first window, Qt
// resources compiled into a static library need registering - so a frontend
// has to be constructible, and able to speak, before there is an application
// object. Hence prepareEnvironment() on an object that owns no widgets yet,
// then createApplication(), which is the frontend's own choice: the GUI needs
// SingleApplication, the CLI only a QCoreApplication and no GUI connection.
//
// This is deliberately shaped so a QPluginLoader-based loader could supply
// frontends later without any of them changing: pure virtual, no constructor
// arguments, metadata in a struct rather than in the constructor.
class ICrowFrontend
{
public:
    virtual ~ICrowFrontend() = default;

    // Runs before any QCoreApplication exists. Anything touching Qt's
    // application state belongs in run(), not here.
    virtual void prepareEnvironment()
    {
    }

    // The application object this frontend needs. Owned by main() so it
    // outlives everything the frontend builds on top of it.
    virtual std::unique_ptr<QCoreApplication> createApplication(int &argc, char **argv) = 0;

    // Build whatever this frontend presents and start the event loop.
    // Returns the process exit code.
    virtual int run(QCoreApplication &app) = 0;
};

// Everything needed to choose a frontend without constructing one.
struct FrontendInfo {
    QString id;
    QString displayName;
};

#endif // ICROWFRONTEND_H
