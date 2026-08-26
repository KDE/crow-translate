/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atranslationprovider.h"

bool ATranslationProvider::redirect(const Language &destination)
{
    Q_UNUSED(destination)
    return false;
}

QString ATranslationProvider::getErrorString()
{
    switch (error) {
    case TranslationError::NoError:
        return QString();
    case TranslationError::UnsupportedSrcLanguage:
        return tr("Unsupported source language.");
    case TranslationError::UnsupportedDstLanguage:
        return tr("Unsupported destination language.");
    case TranslationError::Aborted:
        return tr("User aborted translation in progress");
    case TranslationError::Custom:
        return errorString;
    }
    return tr("Unknown error");
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
        result.clear();
        state = State::Ready;
        emit stateChanged(state);
        break;
    }
}

ATranslationProvider::ATranslationProvider(QObject *parent)
    : QObject{parent}
{
}
