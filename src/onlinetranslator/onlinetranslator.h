/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ONLINETRANSLATOR_H
#define ONLINETRANSLATOR_H

#include "translationexample.h"
#include "translationoptions.h"

#include <QJsonDocument>
#include <QMap>
#include <QPointer>
#include <QUuid>
#include <QVector>

class QStateMachine;
class QState;
class QNetworkAccessManager;
class QNetworkReply;

/**
 * @brief Provides translation data
 */
class OnlineTranslator : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OnlineTranslator)

    friend class OnlineTts;

public:
    /**
     * @brief Represents all languages for translation
     */
    enum Language {
        NoLanguage = -1,
        Auto,
        Afrikaans,
        Albanian,
        Amharic,
        Arabic,
        Armenian,
        Assamese,
        Aymara,
        Azerbaijani,
        Bajan,
        BalkanGipsy,
        Bambara,
        Bangla,
        Bashkir,
        Basque,
        Belarusian,
        Bemba,
        Bhojpuri,
        Bislama,
        Bosnian,
        Breton,
        Bulgarian,
        Cantonese,
        Catalan,
        Cebuano,
        Chamorro,
        Chichewa,
        ChineseLiterary,
        ChineseSimplified,
        ChineseTraditional,
        Chuvash,
        Comorian,
        Coptic,
        Corsican,
        AntiguanCreole,
        BahamianCreole,
        GrenadianCreole,
        GuyaneseCreole,
        JamaicanCreole,
        VincentianCreole,
        VirginIslandsCreole,
        SaintLucianCreole,
        SeselwaCreole,
        UpperGuineaCreole,
        Croatian,
        Czech,
        Danish,
        Dari,
        Divehi,
        Dogri,
        Dutch,
        Dzongkha,
        Elvish,
        Emoji,
        English,
        Esperanto,
        Estonian,
        Ewe,
        Fanagalo,
        Faroese,
        Fijian,
        Filipino,
        Finnish,
        French,
        FrenchCanada,
        Frisian,
        Galician,
        Ganda,
        Georgian,
        German,
        Greek,
        GreekClassical,
        Guarani,
        Gujarati,
        HaitianCreole,
        Hausa,
        Hawaiian,
        Hebrew,
        HillMari,
        Hindi,
        Hmong,
        HmongDaw,
        Hungarian,
        Icelandic,
        Igbo,
        Ilocano,
        Indonesian,
        Inuinnaqtun,
        Inuktitut,
        InuktitutGreenlandic,
        InuktitutLatin,
        Irish,
        Italian,
        Japanese,
        Javanese,
        Kabuverdianu,
        Kabylian,
        Kannada,
        Kazakh,
        KazakhLatin,
        Khmer,
        Kinyarwanda,
        Kirundi,
        Klingon,
        Konkani,
        Korean,
        Krio,
        KurdishCentral,
        KurdishNorthern,
        KurdishSorani,
        Kyrgyz,
        Lao,
        Latin,
        Latvian,
        Lingala,
        Lithuanian,
        LowerSorbian,
        Luganda,
        Luxembourgish,
        Macedonian,
        Maithili,
        Malagasy,
        Malay,
        Malayalam,
        Maltese,
        ManxGaelic,
        Marathi,
        Mari,
        Marshallese,
        Meiteilon,
        Mende,
        Mizo,
        Mongolian,
        MongolianCyrillic,
        MongolianTraditional,
        Morisyen,
        Myanmar,
        Maori,
        Nepali,
        Niuean,
        Norwegian,
        Nyanja,
        Odia,
        Oromo,
        Palauan,
        Papiamentu,
        Pashto,
        Persian,
        Pijin,
        Polish,
        PortugueseBrazilian,
        PortuguesePortugal,
        Potawatomi,
        Punjabi,
        Quechua,
        QueretaroOtomi,
        Romanian,
        Rundi,
        Russian,
        Samoan,
        Sango,
        Sanskrit,
        ScotsGaelic,
        SerbianCyrillic,
        SerbianLatin,
        Sesotho,
        SesothoSaLeboa,
        Setswana,
        Shona,
        Sindhi,
        Sinhala,
        Slovak,
        Slovenian,
        Somali,
        Spanish,
        SrananTongo,
        Sundanese,
        Swahili,
        Swedish,
        Syriac,
        Tahitian,
        Tajik,
        Tamashek,
        Tamil,
        Tatar,
        Telugu,
        Tetum,
        Thai,
        Tibetan,
        Tigrinya,
        TokPisin,
        Tokelauan,
        Tongan,
        Tsonga,
        Turkish,
        Turkmen,
        Tuvaluan,
        Twi,
        Udmurt,
        Ukrainian,
        Uma,
        UpperSorbian,
        Urdu,
        Uyghur,
        UzbekCyrillic,
        UzbekLatin,
        Vietnamese,
        Wallisian,
        Welsh,
        Wolof,
        Xhosa,
        Yakut,
        Yiddish,
        Yoruba,
        YucatecMaya,
        Zulu,
    };
    Q_ENUM(Language)

    /**
     * @brief Represents online engines
     */
    enum Engine {
        Google,
        Yandex,
        Deepl,
        Duckduckgo, // Also known as Bing
        LibreTranslate,
        Mymemory,
        Reverso
    };
    Q_ENUM(Engine)

    /**
     * @brief Indicates all possible error conditions found during the processing of the translation
     */
    enum TranslationError {
        /** No error condition */
        NoError,
        /** Network error */
        NetworkError,
        /** Instance returned an error */
        InstanceError,
        /** The request could not be parsed (report a bug if you see this) */
        ParsingError
    };

    /**
     * @brief Create object
     *
     * Constructs an object with empty data and with parent.
     * You can use translate() to send text to object.
     *
     * @param parent parent object
     */
    explicit OnlineTranslator(QObject *parent = nullptr);

    /**
     * @brief Translate text
     *
     * @param text text to translate
     * @param engine online engine to use
     * @param translationLang language to translation
     * @param sourceLang language of the passed text
     * @param uiLang ui language to use for display
     */
    void translate(const QString &text, Engine engine = Google, Language translationLang = Auto, Language sourceLang = Auto, Language uiLang = Auto);

    /**
     * @brief Detect language
     *
     * @param text text for language detection
     * @param engine engine to use
     */
    void detectLanguage(const QString &text, Engine engine = Google);

    /**
     * @brief Cancel translation operation (if any).
     */
    void abort();

    /**
     * @brief Check translation progress
     *
     * @return `true` when the translation is still processing and has not finished or was aborted yet.
     */
    bool isRunning() const;

    /**
     * @brief Converts the object to JSON
     *
     * @return JSON representation
     */
    QJsonDocument jsonResponse() const;

    /**
     * @brief Source text
     *
     * @return source text
     */
    const QString &source() const;

    /**
     * @brief Source transliteration
     *
     * @return transliteration of the source text
     */
    const QString &sourceTranslit() const;

    /**
     * @brief Source transcription
     *
     * @return transcription of the source text
     */
    const QString &sourceTranscription() const;

    /**
     * @brief Source language name
     *
     * @return language name of the source text
     */
    QString sourceLanguageName() const;

    /**
     * @brief Source language
     *
     * @return language of the source text
     */
    Language sourceLanguage() const;

    /**
     * @brief Translated text
     *
     * @return translated text.
     */
    const QString &translation() const;

    /**
     * @brief Translation transliteration
     *
     * @return transliteration of the translated text
     */
    const QString &translationTranslit() const;

    /**
     * @brief Translation language name
     *
     * @return language name of the translated text
     */
    QString translationLanguageName() const;

    /**
     * @brief Translation language
     *
     * @return language of the translated text
     */
    Language translationLanguage() const;

    /**
     * @brief Translation options
     *
     * @return QMap whose key represents the type of speech, and the value is a QVector of translation options
     * @sa TranslationOptions
     */
    const QVector<TranslationOptions> &translationOptions() const;

    /**
     * @brief Translation examples
     *
     * @return QMap whose key represents the type of speech, and the value is a QVector of translation examples
     * @sa TranslationExample
     */
    const QVector<TranslationExample> &examples() const;

    /**
     * @brief Last error
     *
     * Error that was found during the processing of the last translation.
     * If no error was found, returns OnlineTranslator::NoError.
     * The text of the error can be obtained by errorString().
     *
     * @return last error
     */
    TranslationError error() const;

    /**
     * @brief Last error string
     *
     * A human-readable description of the last translation error that occurred.
     *
     * @return last error string
     */
    const QString &errorString() const;

    /**
     * @brief Check if source transliteration is enabled
     *
     * @return `true` if source transliteration is enabled
     */
    bool isSourceTranslitEnabled() const;

    /**
     * @brief Enable or disable source transliteration
     *
     * @param enable whether to enable source transliteration
     */
    void setSourceTranslitEnabled(bool enable);

    /**
     * @brief Check if translation transliteration is enabled
     *
     * @return `true` if translation transliteration is enabled
     */
    bool isTranslationTranslitEnabled() const;

    /**
     * @brief Enable or disable translation transliteration
     *
     * @param enable whether to enable translation transliteration
     */
    void setTranslationTranslitEnabled(bool enable);

    /**
     * @brief Check if source transcription is enabled
     *
     * @return `true` if source transcription is enabled
     */
    bool isSourceTranscriptionEnabled() const;

    /**
     * @brief Enable or disable source transcription
     *
     * @param enable whether to enable source transcription
     */
    void setSourceTranscriptionEnabled(bool enable);

    /**
     * @brief Check if translation options are enabled
     *
     * @return `true` if translation options are enabled
     * @sa TranslationOptions
     */
    bool isTranslationOptionsEnabled() const;

    /**
     * @brief Enable or disable translation options
     *
     * @param enable whether to enable translation options
     * @sa TranslationOptions
     */
    void setTranslationOptionsEnabled(bool enable);

    /**
     * @brief Check if translation examples are enabled
     *
     * @return `true` if translation examples are enabled
     * @sa TranslationExample
     */
    bool isExamplesEnabled() const;

    /**
     * @brief Enable or disable translation examples
     *
     * @param enable whether to enable translation examples
     * @sa TranslationExample
     */
    void setExamplesEnabled(bool enable);

    /**
     * @brief Returns currenly used instance URL
     *
     * You need to call this function to specify the URL of an instance for Mozhi.
     *
     * @return Mozhi instance url
     */
    const QString &instanceUrl();

    /**
     * @brief Set the URL engine
     *
     * You need to call this function to specify the URL of an instance for Mozhi.
     *
     * @param url Mozhi instance url
     */
    void setInstanceUrl(QString url);

    /**
     * @brief Language name
     *
     * @param lang language
     * @return language name
     */
    static QString languageName(Language lang);

    /**
     * @brief Language code
     *
     * @param lang language
     * @return language code
     */
    static QString languageCode(Language lang);

    /**
     * @brief Language
     *
     * @param locale locale
     * @return language
     */
    static Language language(const QLocale &locale);

    /**
     * @brief Language
     *
     * @param langCode code
     * @return language
     */
    static Language language(const QString &langCode);

signals:
    /**
     * @brief Translation finished
     *
     * This signal is called when the translation is complete.
     */
    void finished();

private slots:
    void skipGarbageText();

    // Google
    void requestTranslate();
    void parseTranslate();

private:
    /*
     * Engines have translation limit, so need to split all text into parts and make request sequentially.
     * Also Yandex and Bing requires several requests to get dictionary, transliteration etc.
     * We use state machine to rely async computation with signals and slots.
     */
    void buildStateMachine();
    void buildDetectStateMachine();

    // Helper functions to build nested states
    void buildSplitNetworkRequest(QState *parent, void (OnlineTranslator::*requestMethod)(), void (OnlineTranslator::*parseMethod)(), const QString &text, int textLimit);
    void buildNetworkRequestState(QState *parent, void (OnlineTranslator::*requestMethod)(), void (OnlineTranslator::*parseMethod)(), const QString &text = {});

    void resetData(TranslationError error = NoError, const QString &errorString = {});

    // Other
    static QString languageApiCode(Engine engine, Language lang);
    static Language language(Engine engine, const QString &langCode);
    static int getSplitIndex(const QString &untranslatedText, int limit);
    static bool isContainsSpace(const QString &text);
    static void addSpaceBetweenParts(QString &text);

    static const QMap<Language, QString> s_genericLanguageCodes;

    // Engines have some language codes exceptions
    static const QMap<Language, QString> s_googleLanguageCodes;
    static const QMap<Language, QString> s_yandexLanguageCodes;
    static const QMap<Language, QString> s_ddgLanguageCodes;
    static const QMap<Language, QString> s_reverso;
    static const QMap<Language, QString> s_mymemory;

    static const int s_textLimits = 100000;

    // This properties used to store unseful information in states
    static constexpr char s_textProperty[] = "Text";

    QStateMachine *m_stateMachine;
    QNetworkAccessManager *m_networkManager;
    QPointer<QNetworkReply> m_currentReply;

    Language m_sourceLang = NoLanguage;
    Language m_translationLang = NoLanguage;
    Language m_uiLang = NoLanguage;
    TranslationError m_error = NoError;
    Engine m_engine = Google;

    QString m_source;
    QString m_sourceTranslit;
    QString m_sourceTranscription;
    QString m_translation;
    QString m_translationTranslit;
    QString m_errorString;

    // Self-hosted Mozhi settings
    QString m_instanceUrl;

    QVector<TranslationOptions> m_translationOptions;
    QVector<TranslationExample> m_examples;
    QJsonDocument m_jsonResponse;

    bool m_sourceTranslitEnabled = true;
    bool m_translationTranslitEnabled = true;
    bool m_sourceTranscriptionEnabled = true;
    bool m_translationOptionsEnabled = true;
    bool m_examplesEnabled = true;

    bool m_onlyDetectLanguage = false;
};

#endif // ONLINETRANSLATOR_H
