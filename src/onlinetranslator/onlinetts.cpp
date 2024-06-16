/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "onlinetranslator/onlinetts.h"

#include <QMetaEnum>
#include <QUrl>

const QMap<OnlineTts::Emotion, QString> OnlineTts::s_emotionCodes = {
    {Neutral, QStringLiteral("neutral")},
    {Good, QStringLiteral("good")},
    {Evil, QStringLiteral("evil")}};

const QMap<OnlineTts::Voice, QString> OnlineTts::s_voiceCodes = {
    {Zahar, QStringLiteral("zahar")},
    {Ermil, QStringLiteral("ermil")},
    {Jane, QStringLiteral("jane")},
    {Oksana, QStringLiteral("oksana")},
    {Alyss, QStringLiteral("alyss")},
    {Omazh, QStringLiteral("omazh")}};

const QMap<QPair<OnlineTranslator::Language, QLocale::Country>, QString> OnlineTts::s_regionCodes = {
    {{OnlineTranslator::Bengali, QLocale::Bangladesh}, QStringLiteral("bn-BD")},
    {{OnlineTranslator::Bengali, QLocale::India}, QStringLiteral("bn-IN")},
    {{OnlineTranslator::SimplifiedChinese, QLocale::China}, QStringLiteral("cmn-Hans-CN")},
    {{OnlineTranslator::English, QLocale::Australia}, QStringLiteral("en-AU")},
    {{OnlineTranslator::English, QLocale::India}, QStringLiteral("en-IN")},
    {{OnlineTranslator::English, QLocale::UnitedKingdom}, QStringLiteral("en-GB")},
    {{OnlineTranslator::English, QLocale::UnitedStates}, QStringLiteral("en-US")},
    {{OnlineTranslator::French, QLocale::Canada}, QStringLiteral("fr-CA")},
    {{OnlineTranslator::French, QLocale::France}, QStringLiteral("fr-FR")},
    {{OnlineTranslator::German, QLocale::Germany}, QStringLiteral("de-DE")},
    {{OnlineTranslator::Portuguese, QLocale::Brazil}, QStringLiteral("pt-BR")},
    {{OnlineTranslator::Spanish, QLocale::Spain}, QStringLiteral("es-ES")},
    {{OnlineTranslator::Spanish, QLocale::UnitedStates}, QStringLiteral("es-US")},
    {{OnlineTranslator::Tamil, QLocale::India}, QStringLiteral("ta-IN")}};

const QMap<OnlineTranslator::Language, QList<QLocale::Country>> OnlineTts::s_validRegions = {
    {OnlineTranslator::Bengali, {QLocale::Bangladesh, QLocale::India}},
    {OnlineTranslator::SimplifiedChinese, {QLocale::China}},
    {OnlineTranslator::English, {QLocale::Australia, QLocale::India, QLocale::UnitedKingdom, QLocale::UnitedStates}},
    {OnlineTranslator::French, {QLocale::Canada, QLocale::France}},
    {OnlineTranslator::German, {QLocale::Germany}},
    {OnlineTranslator::Portuguese, {QLocale::Brazil}},
    {OnlineTranslator::Spanish, {QLocale::Spain, QLocale::UnitedStates}},
    {OnlineTranslator::Tamil, {QLocale::India}}};

OnlineTts::OnlineTts(QObject *parent)
    : QObject(parent)
{
}

void OnlineTts::generateUrls(const QString &text, OnlineTranslator::Engine engine, OnlineTranslator::Language lang, Voice voice, Emotion emotion)
{
    // Get speech
    QString unparsedText = text;
    switch (engine) {
    case OnlineTranslator::Google: {
        if (voice != NoVoice) {
            setError(UnsupportedVoice, tr("Selected engine %1 does not support voice settings").arg(QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)));
            return;
        }

        if (emotion != NoEmotion) {
            setError(UnsupportedEmotion, tr("Selected engine %1 does not support emotion settings").arg(QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)));
            return;
        }

        const QString langString = languageApiCode(engine, lang);
        if (langString.isNull())
            return;

        // Google has a limit of characters per tts request. If the query is larger, then it should be splited into several
        while (!unparsedText.isEmpty()) {
            const int splitIndex = OnlineTranslator::getSplitIndex(unparsedText, s_googleTtsLimit); // Split the part by special symbol

            // Generate URL API for add it to the playlist
            QUrl apiUrl(QStringLiteral("https://translate.googleapis.com/translate_tts"));
            const QString query = QStringLiteral("ie=UTF-8&client=gtx&tl=%1&q=%2").arg(langString, QString(QUrl::toPercentEncoding(unparsedText.left(splitIndex))));
            apiUrl.setQuery(query);
            m_media.append(apiUrl);

            // Remove the said part from the next saying
            unparsedText = unparsedText.mid(splitIndex);
        }
        break;
    }
    case OnlineTranslator::Yandex: {
        const QString langString = languageApiCode(engine, lang);
        if (langString.isNull())
            return;

        const QString voiceString = voiceApiCode(engine, voice);
        if (voiceString.isNull())
            return;

        const QString emotionString = emotionApiCode(engine, emotion);
        if (emotionString.isNull())
            return;

        // Yandex has a limit of characters per tts request. If the query is larger, then it should be splited into several
        while (!unparsedText.isEmpty()) {
            const int splitIndex = OnlineTranslator::getSplitIndex(unparsedText, s_yandexTtsLimit); // Split the part by special symbol

            // Generate URL API for add it to the playlist
            QUrl apiUrl(QStringLiteral("https://tts.voicetech.yandex.net/tts"));
            const QString query = QStringLiteral("text=%1&lang=%2&speaker=%3&emotion=%4&format=mp3")
                                      .arg(QUrl::toPercentEncoding(unparsedText.left(splitIndex)), langString, voiceString, emotionString);
            apiUrl.setQuery(query);
            m_media.append(apiUrl);

            // Remove the said part from the next saying
            unparsedText = unparsedText.mid(splitIndex);
        }
        break;
    }
    case OnlineTranslator::Bing:
    case OnlineTranslator::LibreTranslate:
    case OnlineTranslator::Lingva:
        // NOTE:
        // Lingva returns audio in strange format, use placeholder, until we'll figure it out
        //
        // Example: https://lingva.garudalinux.org/api/v1/audio/en/Hello%20World!
        // Will return json with uint bytes array, according to documentation
        // See: https://github.com/TheDavidDelta/lingva-translate#public-apis
        setError(UnsupportedEngine, tr("%1 engine does not support TTS").arg(QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)));
        break;
    }
}

const QList<QMediaContent> &OnlineTts::media() const
{
    return m_media;
}

const QString &OnlineTts::errorString() const
{
    return m_errorString;
}

OnlineTts::TtsError OnlineTts::error() const
{
    return m_error;
}

QString OnlineTts::voiceCode(Voice voice)
{
    return s_voiceCodes.value(voice);
}

QString OnlineTts::regionCode(OnlineTranslator::Language language, QLocale::Country region)
{
    return s_regionCodes.value({language, region}, OnlineTranslator::languageApiCode(OnlineTranslator::Google, language));
}

QString OnlineTts::emotionCode(Emotion emotion)
{
    return s_emotionCodes.value(emotion);
}

OnlineTts::Emotion OnlineTts::emotion(const QString &emotionCode)
{
    return s_emotionCodes.key(emotionCode, NoEmotion);
}

OnlineTts::Voice OnlineTts::voice(const QString &voiceCode)
{
    return s_voiceCodes.key(voiceCode, NoVoice);
}

QPair<OnlineTranslator::Language, QLocale::Country> OnlineTts::region(const QString &regionCode)
{
    return s_regionCodes.key(regionCode, {OnlineTranslator::NoLanguage, QLocale::AnyCountry});
}

const QMap<OnlineTranslator::Language, QList<QLocale::Country>> &OnlineTts::validRegions()
{
    return s_validRegions;
}

void OnlineTts::setError(TtsError error, const QString &errorString)
{
    m_error = error;
    m_errorString = errorString;
}

// Returns engine-specific language code for tts
QString OnlineTts::languageApiCode(OnlineTranslator::Engine engine, OnlineTranslator::Language lang)
{
    switch (engine) {
    case OnlineTranslator::Google:
    case OnlineTranslator::Lingva: // Lingva is a frontend to Google Translate
        if (lang != OnlineTranslator::Auto)
            return regionCode(lang, m_regionPreferences.value(lang)); // Google use the same codes for tts (except 'auto')
        break;
    case OnlineTranslator::Yandex:
        switch (lang) {
        case OnlineTranslator::Russian:
            return QStringLiteral("ru_RU");
        case OnlineTranslator::Tatar:
            return QStringLiteral("tr_TR");
        case OnlineTranslator::English:
            return QStringLiteral("en_GB");
        default:
            break;
        }
        break;
    default:
        break;
    }

    setError(UnsupportedLanguage, tr("Selected language %1 is not supported for %2").arg(QMetaEnum::fromType<OnlineTranslator::Language>().valueToKey(lang), QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)));
    return {};
}

QString OnlineTts::voiceApiCode(OnlineTranslator::Engine engine, Voice voice)
{
    if (engine == OnlineTranslator::Yandex) {
        if (voice == NoVoice)
            return voiceCode(Zahar);
        return voiceCode(voice);
    }

    setError(UnsupportedVoice, tr("Selected voice %1 is not supported for %2").arg(QMetaEnum::fromType<Voice>().valueToKey(voice), QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)));
    return {};
}

QString OnlineTts::emotionApiCode(OnlineTranslator::Engine engine, Emotion emotion)
{
    if (engine == OnlineTranslator::Yandex) {
        if (emotion == NoEmotion)
            return emotionCode(Neutral);
        return emotionCode(emotion);
    }

    setError(UnsupportedEmotion, tr("Selected emotion %1 is not supported by %2").arg(QMetaEnum::fromType<Emotion>().valueToKey(emotion), QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)));
    return {};
}

const QMap<OnlineTranslator::Language, QLocale::Country> &OnlineTts::regions() const
{
    return m_regionPreferences;
}

void OnlineTts::setRegions(const QMap<OnlineTranslator::Language, QLocale::Country> &newRegionPreferences)
{
    m_regionPreferences = newRegionPreferences;
}
