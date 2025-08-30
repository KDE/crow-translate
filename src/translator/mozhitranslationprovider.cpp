/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mozhitranslationprovider.h"

#include "language.h"
#include "provideroptions.h"
#include "settings/appsettings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocale>
#include <QMap>
#include <QMetaEnum>
#include <QSet>

MozhiTranslationProvider::MozhiTranslationProvider(QObject *parent)
    : ATranslationProvider(parent)
    , m_translator(new OnlineTranslator(this))
    , m_engine(OnlineTranslator::LibreTranslate)
    , m_instanceUrl("https://mozhi.aryak.me")
    , m_isDetecting(false)
{
    connect(m_translator, &OnlineTranslator::finished, this, &MozhiTranslationProvider::onTranslationFinished);
    m_translator->setInstance(m_instanceUrl);

    // Register any OnlineTranslator languages that QLocale can't represent
    registerCustomLanguages();
}

MozhiTranslationProvider::~MozhiTranslationProvider() = default;

QString MozhiTranslationProvider::getProviderType() const
{
    return QStringLiteral("MozhiTranslationProvider");
}

QVector<Language> MozhiTranslationProvider::supportedSourceLanguages()
{
    return getAllSupportedLanguages();
}

QVector<Language> MozhiTranslationProvider::supportedDestinationLanguages()
{
    return getAllSupportedLanguages();
}

bool MozhiTranslationProvider::supportsAutodetection() const
{
    return OnlineTranslator::isSupportsAutodetection(m_engine);
}

Language MozhiTranslationProvider::detectLanguage(const QString &text)
{
    qDebug() << "MozhiTranslationProvider::detectLanguage - text:" << text.left(50) << "current state:" << static_cast<int>(state);
    m_isDetecting = true;
    m_translator->detectLanguage(text, m_engine);

    return QLocale::system();
}

void MozhiTranslationProvider::abort()
{
    qDebug() << "MozhiTranslationProvider::abort - forcing abort, current state:" << static_cast<int>(state);

    // Always abort the underlying translator, even if not "running"
    m_translator->abort();
    m_isDetecting = false;

    // Force state transition to aborted
    state = State::Finished;
    error = TranslationError::Aborted;
    errorString = "Translation aborted by user";
    result.clear();

    qDebug() << "MozhiTranslationProvider::abort - emitting stateChanged:" << static_cast<int>(state);
    emit stateChanged(state);
}

void MozhiTranslationProvider::reset()
{
    ATranslationProvider::reset();
}

void MozhiTranslationProvider::setInstance(const QString &instanceUrl)
{
    m_instanceUrl = instanceUrl;
    m_translator->setInstance(instanceUrl);
}

QString MozhiTranslationProvider::instance() const
{
    return m_instanceUrl;
}

void MozhiTranslationProvider::setEngine(OnlineTranslator::Engine engine)
{
    if (m_engine != engine) {
        m_engine = engine;
        emit engineChanged(static_cast<int>(engine));
    }
}

OnlineTranslator::Engine MozhiTranslationProvider::engine() const
{
    return m_engine;
}

void MozhiTranslationProvider::translate(const QString &inputText, const Language &translationLang, const Language &sourceLang)
{
    qDebug() << "MozhiTranslationProvider::translate - state:" << static_cast<int>(state) << "text:" << inputText.left(50) << "srcLang:" << sourceLang.name()
             << "destLang:" << translationLang.name();

    if (state == State::Processing) {
        qDebug() << "MozhiTranslationProvider::translate - REJECTED: already processing";
        return;
    }

    const OnlineTranslator::Language srcLang = toOnlineTranslatorLanguage(sourceLang, m_engine);
    const OnlineTranslator::Language dstLang = toOnlineTranslatorLanguage(translationLang, m_engine);

    sourceLanguage = sourceLang;
    translationLanguage = translationLang;

    state = State::Processing;
    error = TranslationError::NoError;
    result.clear();

    emit stateChanged(state);

    if (dstLang == OnlineTranslator::NoLanguage) {
        state = State::Finished;
        error = TranslationError::UnsupportedDstLanguage;
        errorString = QString("Destination language '%1' is not supported").arg(translationLang.name());
        emit stateChanged(state);
        return;
    }

    if (srcLang == OnlineTranslator::NoLanguage && sourceLang != QLocale::c()) {
        state = State::Finished;
        error = TranslationError::UnsupportedSrcLanguage;
        errorString = QString("Source language '%1' is not supported").arg(sourceLang.name());
        emit stateChanged(state);
        return;
    }

    m_translator->translate(inputText, m_engine, dstLang, srcLang);
}

void MozhiTranslationProvider::onTranslationFinished()
{
    qDebug() << "MozhiTranslationProvider::onTranslationFinished - isDetecting:" << m_isDetecting << "error:" << static_cast<int>(m_translator->error())
             << "state:" << static_cast<int>(state);

    if (m_isDetecting) {
        m_isDetecting = false;
        if (m_translator->error() == OnlineTranslator::NoError) {
            const OnlineTranslator::Language detectedLang = m_translator->sourceLanguage();
            const Language detectedLanguage = fromOnlineTranslatorLanguage(detectedLang, m_engine);
            sourceLanguage = detectedLanguage; // Set the detected language
            error = TranslationError::NoError;
            state = State::Processed;

            qDebug() << "MozhiTranslationProvider::onTranslationFinished - language detected:" << detectedLanguage.name()
                     << "new state:" << static_cast<int>(state);
            emit languageDetected(detectedLanguage, false); // Detection context only
            emit stateChanged(state);
        }
        return;
    }

    if (m_translator->error() == OnlineTranslator::NoError) {
        result = formatTranslationData(m_translator);
        error = TranslationError::NoError;
        state = State::Processed;

        qDebug() << "MozhiTranslationProvider::onTranslationFinished - translation completed, result length:" << result.length()
                 << "new state:" << static_cast<int>(state);

        if (sourceLanguage == QLocale::c()) {
            const OnlineTranslator::Language detectedLang = m_translator->sourceLanguage();
            sourceLanguage = fromOnlineTranslatorLanguage(detectedLang, m_engine);
            qDebug() << "MozhiTranslationProvider::onTranslationFinished - detected source language:" << sourceLanguage.name();

            // Emit languageDetected signal for auto-detected source during translation
            emit languageDetected(sourceLanguage, true); // Translation context
        }
    } else {
        switch (m_translator->error()) {
        case OnlineTranslator::NetworkError:
            error = TranslationError::Custom;
            errorString = "Network error: " + m_translator->errorString();
            break;
        case OnlineTranslator::InstanceError:
            error = TranslationError::Custom;
            errorString = "Instance error: " + m_translator->errorString();
            break;
        case OnlineTranslator::ParsingError:
            error = TranslationError::Custom;
            errorString = "Parsing error: " + m_translator->errorString();
            break;
        case OnlineTranslator::UnsupportedTtsEngine:
            error = TranslationError::Custom;
            errorString = "Unsupported TTS engine: " + m_translator->errorString();
            break;
        default:
            error = TranslationError::Custom;
            errorString = "Unknown error: " + m_translator->errorString();
            break;
        }
        state = State::Finished;
    }

    qDebug() << "MozhiTranslationProvider::onTranslationFinished - emitting stateChanged:" << static_cast<int>(state);
    emit stateChanged(state);
}

OnlineTranslator::Language MozhiTranslationProvider::toOnlineTranslatorLanguage(const Language &language, OnlineTranslator::Engine /*engine*/)
{
    qDebug() << "toOnlineTranslatorLanguage: language.name():" << language.name()
             << "language.toCode():" << language.toCode()
             << "auto language:" << Language::autoLanguage().name();

    // Handle special case for auto-detection
    if (language == Language::autoLanguage()) {
        qDebug() << "toOnlineTranslatorLanguage: returning OnlineTranslator::Auto";
        return OnlineTranslator::Auto;
    }

    // Try using OnlineTranslator's language code conversion
    // Note: Engine-specific mapping is handled internally by OnlineTranslator during translate()
    const QString bcp47Code = language.toCode();
    qDebug() << "toOnlineTranslatorLanguage: trying BCP47 code:" << bcp47Code;
    const OnlineTranslator::Language fromCode = OnlineTranslator::language(bcp47Code);
    qDebug() << "toOnlineTranslatorLanguage: OnlineTranslator::language(" << bcp47Code << ") returned:" << static_cast<int>(fromCode);
    if (fromCode != OnlineTranslator::NoLanguage) {
        return fromCode;
    }

    // Try with QLocale representation if available
    if (language.hasQLocaleEquivalent()) {
        const OnlineTranslator::Language fromLocale = OnlineTranslator::language(language.toQLocale());
        qDebug() << "toOnlineTranslatorLanguage: trying QLocale lookup, result:" << static_cast<int>(fromLocale);
        if (fromLocale != OnlineTranslator::NoLanguage) {
            return fromLocale;
        }
    }

    return OnlineTranslator::NoLanguage;
}

Language MozhiTranslationProvider::fromOnlineTranslatorLanguage(OnlineTranslator::Language lang, OnlineTranslator::Engine engine)
{
    Q_UNUSED(engine);
    if (lang == OnlineTranslator::Auto || lang == OnlineTranslator::NoLanguage) {
        return Language::autoLanguage();
    }

    const QString langCode = OnlineTranslator::languageCode(lang);
    if (!langCode.isEmpty()) {
        const Language codeLanguage = Language(langCode);
        if (codeLanguage != Language::autoLanguage()) {
            return codeLanguage;
        }
    }

    const QString langName = OnlineTranslator::languageName(lang);
    if (!langName.isEmpty()) {
        const auto availableLanguages = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyCountry);
        for (const QLocale &locale : availableLanguages) {
            if (QLocale::languageToString(locale.language()) == langName) {
                return Language(locale);
            }
        }
    }

    return Language::autoLanguage();
}

QVector<Language> MozhiTranslationProvider::getAllSupportedLanguages() const
{
    QVector<Language> supportedLanguages;
    QSet<QString> addedLanguageCodes;

    for (int i = static_cast<int>(OnlineTranslator::Auto) + 1; i <= static_cast<int>(OnlineTranslator::Zulu); ++i) {
        OnlineTranslator::Language otLang = static_cast<OnlineTranslator::Language>(i);

        const QString langCode = OnlineTranslator::languageCode(otLang);

        if (!langCode.isEmpty() && !addedLanguageCodes.contains(langCode)) {
            addedLanguageCodes.insert(langCode);

            const Language language = Language(langCode);

            if (language != Language::autoLanguage()) {
                supportedLanguages.append(language);
            }
        }
    }

    return supportedLanguages;
}

void MozhiTranslationProvider::applyOptions(const ProviderOptions &options)
{
    if (options.hasOption("instance")) {
        setInstance(options.getOption("instance").toString());
    }

    if (options.hasOption("engine")) {
        bool ok = false;
        const int engineValue = options.getOption("engine").toInt(&ok);
        if (ok) {
            setEngine(static_cast<OnlineTranslator::Engine>(engineValue));
        }
    }
}

std::unique_ptr<ProviderOptions> MozhiTranslationProvider::getDefaultOptions() const
{
    auto options = std::make_unique<ProviderOptions>();
    options->setOption("instance", "https://mozhi.aryak.me");
    options->setOption("engine", static_cast<int>(OnlineTranslator::LibreTranslate));
    return options;
}

QStringList MozhiTranslationProvider::getAvailableOptions() const
{
    return {"instance", "engine"};
}

ProviderUIRequirements MozhiTranslationProvider::getUIRequirements() const
{
    ProviderUIRequirements requirements;
    requirements.requiredUIElements = {"engineComboBox"};
    requirements.supportedSignals = {"engineChanged", "languageDetected"};
    requirements.supportedCapabilities = {"languageDetection", "engineSelection"};
    return requirements;
}

void MozhiTranslationProvider::saveOptionToSettings(const QString &optionKey, const QVariant &value)
{
    AppSettings settings;
    if (optionKey == "engine") {
        settings.setCurrentEngine(static_cast<OnlineTranslator::Engine>(value.toInt()));
    } else if (optionKey == "instance") {
        settings.setInstance(value.toString());
    }
}

QString MozhiTranslationProvider::formatTranslationData(OnlineTranslator *translator)
{
    QString formattedResult;

    QString translation = translator->translation();

    for (int i = 0; i < translation.size(); ++i) {
        if (translation[i].category() == QChar::Symbol_Other) {
            translation.remove(i, 1);
            --i;
        }
    }

    formattedResult = translation.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>"));

    if (!translator->translationTranslit().isEmpty()) {
        QString translit = translator->translationTranslit();
        formattedResult += QStringLiteral("<br><font color=\"grey\"><i>/%1/</i></font>").arg(translit.replace(QStringLiteral("\n"), QStringLiteral("/<br>/")));
    }

    if (!translator->sourceTranslit().isEmpty()) {
        QString translit = translator->sourceTranslit();
        formattedResult +=
            QStringLiteral("<br><font color=\"grey\"><i><b>(%1)</b></i></font>").arg(translit.replace(QStringLiteral("\n"), QStringLiteral("/<br>/")));
    }

    if (!translator->sourceTranscription().isEmpty()) {
        formattedResult += QStringLiteral("<br><font color=\"grey\">[%1]</font>").arg(translator->sourceTranscription());
    }

    if (!translator->translationOptions().isEmpty()) {
        formattedResult += QStringLiteral("<br><br><b>%1</b><br>").arg(tr("translation options:"));

        for (const auto &[word, translations] : translator->translationOptions()) {
            QString wordLine = QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;") + word;

            if (!translations.isEmpty()) {
                wordLine += QStringLiteral(": <font color=\"grey\"><i>%1</i></font>").arg(translations.join(QStringLiteral(", ")));
            }

            formattedResult += wordLine + QStringLiteral("<br>");
        }
    }

    if (!translator->examples().isEmpty()) {
        formattedResult += QStringLiteral("<br><b>%1</b><br>").arg(tr("examples:"));

        for (const auto &[word, example, definition, examplesSource, examplesTarget] : translator->examples()) {
            formattedResult += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;<i>%1</i><br>").arg(word);

            if (!definition.isEmpty()) {
                formattedResult += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;%1<br>").arg(definition);
            }

            if (!example.isEmpty()) {
                formattedResult += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;<font color=\"grey\"><i>%1</i></font><br>").arg(example);
            }

            for (qsizetype i = 0; i < examplesSource.size(); ++i) {
                formattedResult += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;%1 <font color=\"grey\"><i>%2</i></font><br>")
                                       .arg(examplesSource[i].toHtmlEscaped(), examplesTarget[i].toHtmlEscaped());
            }

            formattedResult += QStringLiteral("<br>");
        }
    }

    return formattedResult;
}

void MozhiTranslationProvider::registerCustomLanguages()
{
    qDebug() << "MozhiTranslationProvider: Registering custom languages for OnlineTranslator languages not in QLocale";

    static bool registered = false;
    if (registered) {
        return; // Only register once
    }
    registered = true;

    // Go through all OnlineTranslator languages and register ones that QLocale can't represent
    for (int i = static_cast<int>(OnlineTranslator::Auto) + 1; i <= static_cast<int>(OnlineTranslator::Zulu); ++i) {
        OnlineTranslator::Language otLang = static_cast<OnlineTranslator::Language>(i);
        const QString langCode = OnlineTranslator::languageCode(otLang);
        const QString langName = OnlineTranslator::languageName(otLang);

        if (langCode.isEmpty() || langName.isEmpty()) {
            continue; // Skip invalid languages
        }

        // Debug Klingon registration
        if (otLang == OnlineTranslator::Klingon) {
            qDebug() << "MozhiTranslationProvider: Registering Klingon - otLang:" << static_cast<int>(otLang)
                     << "langCode:" << langCode << "langName:" << langName;
        }

        // Try QLocale with the language code (handles BCP47 like zh-TW, pt-PT, mn-Cyrl)
        QLocale testLocale(langCode);
        if (testLocale != QLocale::c()) {
            // QLocale can represent this language
            continue;
        }

        // Also try QLocale's language code conversion methods
        QLocale::Language qLang = QLocale::codeToLanguage(langCode, QLocale::AnyLanguageCode);
        if (qLang != QLocale::AnyLanguage) {
            // QLocale can represent this language
            continue;
        }

        // This language is truly not representable in QLocale, register it as custom
        qDebug() << "  Registering custom language:" << langCode << "(" << langName << ")";

        // Extract potential ISO639 codes from the language code
        QString iso639_1, iso639_2;
        QStringList parts = langCode.split('-');
        if (!parts.isEmpty()) {
            QString baseLang = parts.first().toLower();
            if (baseLang.length() == 2) {
                iso639_1 = baseLang;
            } else if (baseLang.length() == 3) {
                iso639_2 = baseLang;
            }
        }

        // Debug Klingon registration details
        if (otLang == OnlineTranslator::Klingon) {
            qDebug() << "MozhiTranslationProvider: About to register Klingon with code:" << langCode
                     << "name:" << langName << "iso639_1:" << iso639_1 << "iso639_2:" << iso639_2;
        }

        Language::registerCustomLanguage(langCode, langName, iso639_1, iso639_2);
    }

    qDebug() << "MozhiTranslationProvider: Finished registering custom languages";
}
