/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "providerfactories.h"

#include "core/usernotifier.h"
#include "settings/appsettings.h"
#include "tts/noopttsprovider.h"
#ifdef WITH_TTS
#include "tts/mozhittsprovider.h"
#include "tts/qtttsprovider.h"
#endif
#ifdef WITH_PIPER_TTS
#include "tts/piperttsprovider.h"
#endif

#include <QDebug>

namespace
{

void resetProblematicTTSProvider(ATTSProvider::ProviderBackend backend)
{
    // Reset the problematic provider to None in settings to prevent future crashes
    AppSettings settings;
    qWarning() << "Resetting TTS provider from" << static_cast<int>(backend) << "to None due to crash";
    settings.setTTSProviderBackend(ATTSProvider::ProviderBackend::None);

    UserNotifier::Notification notification;
    notification.severity = UserNotifier::Severity::Warning;
    notification.title = QObject::tr("TTS Provider Error");
    notification.text = QObject::tr("The selected TTS provider crashed during initialization and has been reset to 'None' to prevent further issues. "
                                    "You can try selecting a different provider in Settings → TTS.");
    UserNotifier::notify(notification);
}

} // namespace

ATTSProvider *ProviderFactories::createTTSProvider(QObject *parent, ATTSProvider::ProviderBackend chosenBackend)
{
    // Try to create the requested provider with crash protection
    try {
        switch (chosenBackend) {
        case ATTSProvider::ProviderBackend::None:
            return new NoopTTSProvider(parent);
#ifdef WITH_TTS
        case ATTSProvider::ProviderBackend::Qt:
            return new QtTTSProvider(parent);
        case ATTSProvider::ProviderBackend::Mozhi:
            return new MozhiTTSProvider(parent);
#endif
        case ATTSProvider::ProviderBackend::Piper:
#ifdef WITH_PIPER_TTS
            return new PiperTTSProvider(parent);
#else
            return new NoopTTSProvider(parent);
#endif
        }
    } catch (const std::exception &e) {
        qWarning() << "ProviderFactories::createTTSProvider - Exception creating provider" << static_cast<int>(chosenBackend) << ":" << e.what();
        qWarning() << "Falling back to None provider to prevent crash";

        // Reset the problematic provider in settings to prevent future crashes
        resetProblematicTTSProvider(chosenBackend);

        return new NoopTTSProvider(parent);
    } catch (...) {
        qWarning() << "ProviderFactories::createTTSProvider - Unknown exception creating provider" << static_cast<int>(chosenBackend);
        qWarning() << "Falling back to None provider to prevent crash";

        // Reset the problematic provider in settings to prevent future crashes
        resetProblematicTTSProvider(chosenBackend);

        return new NoopTTSProvider(parent);
    }

    Q_UNREACHABLE();
}
