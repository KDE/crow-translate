/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "attsprovider.h"

#include "tts/mozhittsprovider.h"
#include "tts/noopttsprovider.h"
#include "tts/qtttsprovider.h"
#ifdef WITH_PIPER_TTS
#include "tts/piperttsprovider.h"
#endif

#include "settings/appsettings.h"

#include <QMessageBox>
#include <QTimer>

ATTSProvider *ATTSProvider::createTTSProvider(QObject *parent, ATTSProvider::ProviderBackend chosenBackend)
{
    // Try to create the requested provider with crash protection
    try {
        switch (chosenBackend) {
        case ProviderBackend::None:
            return new NoopTTSProvider(parent);
        case ProviderBackend::Qt:
            return new QtTTSProvider(parent);
        case ProviderBackend::Mozhi:
            return new MozhiTTSProvider(parent);
        case ProviderBackend::Piper:
#ifdef WITH_PIPER_TTS
            return new PiperTTSProvider(parent);
#else
            return new NoopTTSProvider(parent);
#endif
        }
    } catch (const std::exception &e) {
        qWarning() << "ATTSProvider::createTTSProvider - Exception creating provider" << static_cast<int>(chosenBackend) << ":" << e.what();
        qWarning() << "Falling back to None provider to prevent crash";

        // Reset the problematic provider in settings to prevent future crashes
        resetProblematicProvider(chosenBackend);

        return new NoopTTSProvider(parent);
    } catch (...) {
        qWarning() << "ATTSProvider::createTTSProvider - Unknown exception creating provider" << static_cast<int>(chosenBackend);
        qWarning() << "Falling back to None provider to prevent crash";

        // Reset the problematic provider in settings to prevent future crashes
        resetProblematicProvider(chosenBackend);

        return new NoopTTSProvider(parent);
    }

    Q_UNREACHABLE();
}

void ATTSProvider::resetProblematicProvider(ProviderBackend backend)
{
    // Reset the problematic provider to None in settings to prevent future crashes
    AppSettings settings;
    qWarning() << "Resetting TTS provider from" << static_cast<int>(backend) << "to None due to crash";
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::None);

    // Show a user-friendly message
    QTimer::singleShot(100, []() {
        QMessageBox::warning(nullptr,
                             QObject::tr("TTS Provider Error"),
                             QObject::tr("The selected TTS provider crashed during initialization and has been reset to 'None' to prevent further issues. "
                                         "You can try selecting a different provider in Settings → TTS."));
    });
}

ATTSProvider::ATTSProvider(QObject *parent)
    : QObject{parent}
{
}
