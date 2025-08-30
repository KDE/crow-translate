/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atranslationprovider.h"

#include "settings/appsettings.h"
#include "translator/copytranslationprovider.h"
#include "translator/mozhitranslationprovider.h"

#include <QMessageBox>
#include <QTimer>

#include <variant>

ATranslationProvider *ATranslationProvider::createTranslationProvider(QObject *parent, ATranslationProvider::ProviderBackend chosenBackend)
{
    // Try to create the requested provider with crash protection
    try {
        switch (chosenBackend) {
        case ProviderBackend::Copy:
            return new CopyTranslationProvider(parent);

        case ProviderBackend::Mozhi:
            return new MozhiTranslationProvider(parent);
        }
    } catch (const std::exception &e) {
        qWarning() << "ATranslationProvider::createTranslationProvider - Exception creating provider" << static_cast<int>(chosenBackend) << ":" << e.what();
        qWarning() << "Falling back to Copy provider to prevent crash";

        // Reset the problematic provider in settings to prevent future crashes
        resetProblematicProvider(chosenBackend);

        return new CopyTranslationProvider(parent);
    } catch (...) {
        qWarning() << "ATranslationProvider::createTranslationProvider - Unknown exception creating provider" << static_cast<int>(chosenBackend);
        qWarning() << "Falling back to Copy provider to prevent crash";

        // Reset the problematic provider in settings to prevent future crashes
        resetProblematicProvider(chosenBackend);

        return new CopyTranslationProvider(parent);
    }
    return nullptr;
}
QString ATranslationProvider::getErrorString()
{
    switch (error) {
    case TranslationError::NoError:
        return QString();
    case TranslationError::UnsupportedSrcLanguage:
        return QString("Unsupported source language.");
    case TranslationError::UnsupportedDstLanguage:
        return QString("Unsupported destination language.");
    case TranslationError::Aborted:
        return QString("User aborted translation in progress");
    case TranslationError::Custom:
        return errorString;
    }
    return QString("Unknown error");
}
ATranslationProvider::State ATranslationProvider::getState()
{
    return state;
}
void ATranslationProvider::finish()
{
    state = State::Finished;
    error = TranslationError::NoError;
    emit stateChanged(state);
}
void ATranslationProvider::abort()
{
    if (state == State::Processing || state == State::Processed) {
        state = State::Finished;
        error = TranslationError::Aborted;
        emit stateChanged(state);
    }
}

void ATranslationProvider::reset()
{
    switch (state) {
    case ATranslationProvider::State::Ready:
        break;
    case ATranslationProvider::State::Processing:
    case ATranslationProvider::State::Processed:
        abort();
        reset();
        break;
    case ATranslationProvider::State::Finished:
        error = TranslationError::NoError;
        errorString.clear();
        result = QString();
        state = State::Ready;
        emit stateChanged(state);
        break;
    }
}

void ATranslationProvider::resetProblematicProvider(ProviderBackend backend)
{
    // Reset the problematic provider to Copy in settings to prevent future crashes
    AppSettings settings;
    qWarning() << "Resetting translation provider from" << static_cast<int>(backend) << "to Copy due to crash";
    settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Copy);

    // Show a user-friendly message
    QTimer::singleShot(100, []() {
        QMessageBox::warning(nullptr,
                             QObject::tr("Translation Provider Error"),
                             QObject::tr("The selected translation provider crashed during initialization and has been reset to 'Copy' to prevent further issues. "
                                         "You can try selecting a different provider in Settings → Translation."));
    });
}

ATranslationProvider::ATranslationProvider(QObject *parent)
    : QObject{parent}
{
}
