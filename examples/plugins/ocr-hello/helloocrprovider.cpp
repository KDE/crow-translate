/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "helloocrprovider.h"

#include "provideroptions.h"

#include <QImage>
#include <QTimer>

namespace
{
// Kept in step with helloocrplugin.cpp, which declares it.
const QString labelOption = QStringLiteral("label");
const QString defaultLabel = QStringLiteral("hello");
}

HelloOcrProvider::HelloOcrProvider(QObject *parent)
    : AOcrProvider(parent)
    , m_label{defaultLabel}
{
}

// Shown to the user as the name of what is running. Free text.
QString HelloOcrProvider::engineName() const
{
    return QStringLiteral("Hello (example)");
}

// Whether this engine could run right now. A real one answers no when its
// language data is missing or its endpoint is unset, and the application then
// tells the user to go and configure it instead of starting something that
// will fail.
bool HelloOcrProvider::isConfigured() const
{
    return true;
}

void HelloOcrProvider::recognize(const QImage &image, int dpi)
{
    Q_UNUSED(dpi)

    m_canceled = false;

    // started() is what the status strip watches, and it has to be emitted
    // before the work rather than with the result: "recognizing" is an
    // interval the user is meant to see, not an instant.
    emit started();

    const QString text = QStringLiteral("%1: %2x%3").arg(m_label).arg(image.width()).arg(image.height());

    // Asynchronously, as both real engines are - one runs Tesseract on a
    // thread, the other waits on an HTTP response. Answering from inside
    // recognize() would work here and stop working the moment somebody
    // replaced the body with something that takes time, so the example does
    // not teach it.
    QTimer::singleShot(0, this, [this, text] {
        if (m_canceled) {
            emit canceled();
            return;
        }
        emit recognized(text);
    });
}

void HelloOcrProvider::cancel()
{
    m_canceled = true;
}

void HelloOcrProvider::applyOptions(const ProviderOptions &options)
{
    if (options.hasOption(labelOption))
        m_label = options.getOption(labelOption).toString();

    // If the options left this engine unable to run - no language data for the
    // languages it was given, say - the place to report it is
    // configurationFailed(), not a message box. Whether a machine with nothing
    // installed deserves to be told is the application's question.
}

QStringList HelloOcrProvider::getAvailableOptions() const
{
    return {labelOption};
}
