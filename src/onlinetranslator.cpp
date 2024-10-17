/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "onlinetranslator.h"

#include <QCoreApplication>
#include <QFinalState>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QNetworkReply>
#include <QStateMachine>

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_genericLanguageCodes = {
    {Auto, QStringLiteral("auto")},
    {Afrikaans, QStringLiteral("af")},
    {Albanian, QStringLiteral("sq")},
    {Amharic, QStringLiteral("am")},
    {Arabic, QStringLiteral("ar")},
    {Armenian, QStringLiteral("hy")},
    {Assamese, QStringLiteral("as")},
    {Aymara, QStringLiteral("ay")},
    {Azerbaijani, QStringLiteral("az")},
    {Bajan, QStringLiteral("bjs")},
    {BalkanGipsy, QStringLiteral("rm")},
    {Bambara, QStringLiteral("bm")},
    {Bangla, QStringLiteral("bn")},
    {Bashkir, QStringLiteral("ba")},
    {Basque, QStringLiteral("eu")},
    {Belarusian, QStringLiteral("be")},
    {Bemba, QStringLiteral("bem")},
    {Bhojpuri, QStringLiteral("bho")},
    {Bislama, QStringLiteral("bi")},
    {Bosnian, QStringLiteral("bs")},
    {Breton, QStringLiteral("br")},
    {Bulgarian, QStringLiteral("bg")},
    {Cantonese, QStringLiteral("yue")},
    {Catalan, QStringLiteral("ca")},
    {Cebuano, QStringLiteral("ceb")},
    {Chamorro, QStringLiteral("ch")},
    {Chichewa, QStringLiteral("ny")},
    {ChineseLiterary, QStringLiteral("lzh")},
    {ChineseSimplified, QStringLiteral("zh")},
    {ChineseTraditional, QStringLiteral("zh-TW")},
    {Chuvash, QStringLiteral("cv")},
    {Comorian, QStringLiteral("zdj")},
    {Coptic, QStringLiteral("cop")},
    {Corsican, QStringLiteral("co")},
    {AntiguanCreole, QStringLiteral("aig")},
    {BahamianCreole, QStringLiteral("bah")},
    {GrenadianCreole, QStringLiteral("gcl")},
    {GuyaneseCreole, QStringLiteral("gyn")},
    {JamaicanCreole, QStringLiteral("jam")},
    {VincentianCreole, QStringLiteral("svc")},
    {VirginIslandsCreole, QStringLiteral("vic")},
    {SaintLucianCreole, QStringLiteral("acf")},
    {SeselwaCreole, QStringLiteral("crs")},
    {UpperGuineaCreole, QStringLiteral("pov")},
    {Croatian, QStringLiteral("hr")},
    {Czech, QStringLiteral("cs")},
    {Danish, QStringLiteral("da")},
    {Dari, QStringLiteral("prs")},
    {Divehi, QStringLiteral("dv")},
    {Dogri, QStringLiteral("doi")},
    {Dutch, QStringLiteral("nl")},
    {Dzongkha, QStringLiteral("dz")},
    {Elvish, QStringLiteral("sjn")},
    {Emoji, QStringLiteral("emj")},
    {English, QStringLiteral("en")},
    {Esperanto, QStringLiteral("eo")},
    {Estonian, QStringLiteral("et")},
    {Ewe, QStringLiteral("ee")},
    {Fanagalo, QStringLiteral("fn")},
    {Faroese, QStringLiteral("fo")},
    {Fijian, QStringLiteral("fj")},
    {Filipino, QStringLiteral("fil")},
    {Filipino, QStringLiteral("tl")},
    {Finnish, QStringLiteral("fi")},
    {French, QStringLiteral("fr")},
    {FrenchCanada, QStringLiteral("fr-CA")},
    {Frisian, QStringLiteral("fy")},
    {Galician, QStringLiteral("gl")},
    {Ganda, QStringLiteral("lug")},
    {Georgian, QStringLiteral("ka")},
    {German, QStringLiteral("de")},
    {Greek, QStringLiteral("el")},
    {GreekClassical, QStringLiteral("grc")},
    {Guarani, QStringLiteral("gn")},
    {Gujarati, QStringLiteral("gu")},
    {HaitianCreole, QStringLiteral("ht")},
    {Hausa, QStringLiteral("ha")},
    {Hawaiian, QStringLiteral("haw")},
    {Hebrew, QStringLiteral("he")},
    {Hebrew, QStringLiteral("iw")},
    {HillMari, QStringLiteral("mrj")},
    {Hindi, QStringLiteral("hi")},
    {Hmong, QStringLiteral("hmn")},
    {HmongDaw, QStringLiteral("mww")},
    {Hungarian, QStringLiteral("hu")},
    {Icelandic, QStringLiteral("is")},
    {Igbo, QStringLiteral("ig")},
    {Ilocano, QStringLiteral("ilo")},
    {Indonesian, QStringLiteral("id")},
    {Inuinnaqtun, QStringLiteral("ikt")},
    {Inuktitut, QStringLiteral("iu")},
    {InuktitutGreenlandic, QStringLiteral("kl")},
    {InuktitutLatin, QStringLiteral("iu-Latn")},
    {Irish, QStringLiteral("ga")},
    {Italian, QStringLiteral("it")},
    {Japanese, QStringLiteral("ja")},
    {Javanese, QStringLiteral("jv")},
    {Javanese, QStringLiteral("jw")},
    {Kabuverdianu, QStringLiteral("kea")},
    {Kabylian, QStringLiteral("kab")},
    {Kannada, QStringLiteral("kn")},
    {Kazakh, QStringLiteral("kk")},
    {KazakhLatin, QStringLiteral("kazlat")},
    {Khmer, QStringLiteral("km")},
    {Kinyarwanda, QStringLiteral("rw")},
    {Kirundi, QStringLiteral("rn")},
    {Klingon, QStringLiteral("tlh-Latn")},
    {Konkani, QStringLiteral("gom")},
    {Korean, QStringLiteral("ko")},
    {Krio, QStringLiteral("kri")},
    {KurdishCentral, QStringLiteral("ku")},
    {KurdishNorthern, QStringLiteral("kmr")},
    {KurdishSorani, QStringLiteral("ckb")},
    {Kyrgyz, QStringLiteral("ky")},
    {Lao, QStringLiteral("lo")},
    {Latin, QStringLiteral("la")},
    {Latvian, QStringLiteral("lv")},
    {Lingala, QStringLiteral("ln")},
    {Lithuanian, QStringLiteral("lt")},
    {LowerSorbian, QStringLiteral("dsb")},
    {Luganda, QStringLiteral("lg")},
    {Luxembourgish, QStringLiteral("lb")},
    {Macedonian, QStringLiteral("mk")},
    {Maithili, QStringLiteral("mai")},
    {Malagasy, QStringLiteral("mg")},
    {Malay, QStringLiteral("ms")},
    {Malayalam, QStringLiteral("ml")},
    {Maltese, QStringLiteral("mt")},
    {ManxGaelic, QStringLiteral("gv")},
    {Marathi, QStringLiteral("mr")},
    {Mari, QStringLiteral("mhr")},
    {Marshallese, QStringLiteral("mh")},
    {Meiteilon, QStringLiteral("mni-Mtei")},
    {Mende, QStringLiteral("men")},
    {Mizo, QStringLiteral("lus")},
    {Mongolian, QStringLiteral("mn")},
    {MongolianCyrillic, QStringLiteral("mn-Cyrl")},
    {MongolianTraditional, QStringLiteral("mn-Mong")},
    {Morisyen, QStringLiteral("mfe")},
    {Myanmar, QStringLiteral("my")},
    {Maori, QStringLiteral("mi")},
    {Nepali, QStringLiteral("ne")},
    {Niuean, QStringLiteral("niu")},
    {Norwegian, QStringLiteral("no")},
    {Nyanja, QStringLiteral("nya")},
    {Odia, QStringLiteral("or")},
    {Oromo, QStringLiteral("om")},
    {Palauan, QStringLiteral("pau")},
    {Papiamentu, QStringLiteral("pap")},
    {Pashto, QStringLiteral("ps")},
    {Persian, QStringLiteral("fa")},
    {Pijin, QStringLiteral("pis")},
    {Polish, QStringLiteral("pl")},
    {PortugueseBrazilian, QStringLiteral("pt")},
    {PortuguesePortugal, QStringLiteral("pt-PT")},
    {Potawatomi, QStringLiteral("pot")},
    {Punjabi, QStringLiteral("pa")},
    {Quechua, QStringLiteral("qu")},
    {QueretaroOtomi, QStringLiteral("otq")},
    {Romanian, QStringLiteral("ro")},
    {Rundi, QStringLiteral("run")},
    {Russian, QStringLiteral("ru")},
    {Samoan, QStringLiteral("sm")},
    {Sango, QStringLiteral("sg")},
    {Sanskrit, QStringLiteral("sa")},
    {ScotsGaelic, QStringLiteral("gd")},
    {SerbianCyrillic, QStringLiteral("sr-Cyrl")},
    {SerbianLatin, QStringLiteral("sr")},
    {Sesotho, QStringLiteral("st")},
    {SesothoSaLeboa, QStringLiteral("nso")},
    {Setswana, QStringLiteral("tn")},
    {Shona, QStringLiteral("sn")},
    {Sindhi, QStringLiteral("sd")},
    {Sinhala, QStringLiteral("si")},
    {Slovak, QStringLiteral("sk")},
    {Slovenian, QStringLiteral("sl")},
    {Somali, QStringLiteral("so")},
    {Spanish, QStringLiteral("es")},
    {SrananTongo, QStringLiteral("srn")},
    {Sundanese, QStringLiteral("su")},
    {Swahili, QStringLiteral("sw")},
    {Swedish, QStringLiteral("sv")},
    {Syriac, QStringLiteral("syc")},
    {Tahitian, QStringLiteral("ty")},
    {Tajik, QStringLiteral("tg")},
    {Tamashek, QStringLiteral("tmh")},
    {Tamil, QStringLiteral("ta")},
    {Tatar, QStringLiteral("tt")},
    {Telugu, QStringLiteral("te")},
    {Tetum, QStringLiteral("tet")},
    {Thai, QStringLiteral("th")},
    {Tibetan, QStringLiteral("bo")},
    {Tigrinya, QStringLiteral("ti")},
    {TokPisin, QStringLiteral("tpi")},
    {Tokelauan, QStringLiteral("tkl")},
    {Tongan, QStringLiteral("to")},
    {Tsonga, QStringLiteral("ts")},
    {Turkish, QStringLiteral("tr")},
    {Turkmen, QStringLiteral("tk")},
    {Tuvaluan, QStringLiteral("tvl")},
    {Twi, QStringLiteral("ak")},
    {Udmurt, QStringLiteral("udm")},
    {Ukrainian, QStringLiteral("uk")},
    {Uma, QStringLiteral("ppk")},
    {UpperSorbian, QStringLiteral("hsb")},
    {Urdu, QStringLiteral("ur")},
    {Uyghur, QStringLiteral("ug")},
    {UzbekCyrillic, QStringLiteral("uzbcyr")},
    {UzbekLatin, QStringLiteral("uz")},
    {Vietnamese, QStringLiteral("vi")},
    {Wallisian, QStringLiteral("wls")},
    {Welsh, QStringLiteral("cy")},
    {Wolof, QStringLiteral("wo")},
    {Xhosa, QStringLiteral("xh")},
    {Yakut, QStringLiteral("sah")},
    {Yiddish, QStringLiteral("yi")},
    {Yoruba, QStringLiteral("yo")},
    {YucatecMaya, QStringLiteral("yua")},
    {Zulu, QStringLiteral("zu")}};

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_googleLanguageCodes = {
    {ChineseSimplified, QStringLiteral("zh-CN")},
    {Hebrew, QStringLiteral("iw")}};

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_yandexLanguageCodes = {
    {SerbianCyrillic, QStringLiteral("sr")},
    {SerbianLatin, QStringLiteral("sr-Latn")},
    {PortugueseBrazilian, QStringLiteral("pt-BR")},
    {PortuguesePortugal, QStringLiteral("pt")},
    {Javanese, QStringLiteral("jv")}};

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_ddgLanguageCodes = {
    {SerbianLatin, QStringLiteral("sr-Latn")},
    {Filipino, QStringLiteral("fil")},
    {SerbianCyrillic, QStringLiteral("sr-Cyrl")},
    {Hmong, QStringLiteral("mww")}};

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_reverso = {{Persian, QStringLiteral("per")}};

const QMap<OnlineTranslator::Language, QString> OnlineTranslator::s_mymemory = {{Auto, QStringLiteral("Autodetect")}, {Norwegian, QStringLiteral("no")}};

OnlineTranslator::OnlineTranslator(QObject *parent)
    : QObject(parent)
    , m_stateMachine(new QStateMachine(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_stateMachine, &QStateMachine::finished, this, &OnlineTranslator::finished);
    connect(m_stateMachine, &QStateMachine::stopped, this, &OnlineTranslator::finished);
}

void OnlineTranslator::translate(const QString &text, Engine engine, Language translationLang, Language sourceLang)
{
    abort();

    if (sourceLang == Auto && !isSupportsAutodetection(engine)) {
        resetData(InstanceError, tr("Language detection is not supported for %1").arg(QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)));
        emit finished();
        return;
    }

    resetData();
    m_onlyDetectLanguage = false;
    m_source = text;
    m_sourceLang = sourceLang;
    m_translationLang = translationLang == Auto ? language(QLocale()) : translationLang;
    m_engine = engine;

    buildStateMachine();
    m_stateMachine->start();
}

void OnlineTranslator::detectLanguage(const QString &text, Engine engine)
{
    abort();
    resetData();

    m_onlyDetectLanguage = true;
    m_source = text;
    m_sourceLang = Auto;
    m_translationLang = English;
    m_engine = engine;

    buildDetectStateMachine();
    m_stateMachine->start();
}

void OnlineTranslator::abort()
{
    if (m_currentReply != nullptr)
        m_currentReply->abort();
}

bool OnlineTranslator::isRunning() const
{
    return m_stateMachine->isRunning();
}

QList<QMediaContent> OnlineTranslator::generateUrls(const QString &text, OnlineTranslator::Engine engine, OnlineTranslator::Language lang)
{
    // Get speech
    QString unparsedText = text;
    switch (engine) {
    case OnlineTranslator::Reverso:
    case OnlineTranslator::Google: {
        m_error = NoError;
        m_errorString.clear();

        const QString langString = OnlineTranslator::languageApiCode(engine, lang);
        const QString engineString = QString(QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine)).toLower();

        // Limit characters per tts request. If the query is larger, then it should be splited into several
        QList<QMediaContent> media;
        while (!unparsedText.isEmpty()) {
            const int splitIndex = OnlineTranslator::getSplitIndex(unparsedText, s_TtsLimit); // Split the part by special symbol

            // Generate URL API for add it to the playlist
            QUrl apiUrl(QStringLiteral("%1/api/tts").arg(m_instance));
            const QString query = QStringLiteral("engine=%1&lang=%2&text=%3").arg(engineString, langString, QString(QUrl::toPercentEncoding(unparsedText.left(splitIndex))));
            apiUrl.setQuery(query);
            media.append(apiUrl);

            // Remove the said part from the next saying
            unparsedText = unparsedText.mid(splitIndex);
        }
        return media;
    }
    case OnlineTranslator::Yandex: // For some reason, Yandex doesn't supprt TTS in Mozhi
    case OnlineTranslator::Deepl:
    case OnlineTranslator::LibreTranslate:
    case OnlineTranslator::Duckduckgo:
    case OnlineTranslator::Mymemory:
        m_error = UnsupportedTtsEngine;
        m_errorString = tr("%1 engine does not support TTS").arg(QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(engine));
        return {};
    }

    Q_UNREACHABLE();
}

QJsonDocument OnlineTranslator::jsonResponse() const
{
    return m_jsonResponse;
}

const QString &OnlineTranslator::source() const
{
    return m_source;
}

const QString &OnlineTranslator::sourceTranslit() const
{
    return m_sourceTranslit;
}

const QString &OnlineTranslator::sourceTranscription() const
{
    return m_sourceTranscription;
}

QString OnlineTranslator::sourceLanguageName() const
{
    return languageName(m_sourceLang);
}

OnlineTranslator::Language OnlineTranslator::sourceLanguage() const
{
    return m_sourceLang;
}

const QString &OnlineTranslator::translation() const
{
    return m_translation;
}

const QString &OnlineTranslator::translationTranslit() const
{
    return m_translationTranslit;
}

QString OnlineTranslator::translationLanguageName() const
{
    return languageName(m_translationLang);
}

OnlineTranslator::Language OnlineTranslator::translationLanguage() const
{
    return m_translationLang;
}

const QVector<TranslationOptions> &OnlineTranslator::translationOptions() const
{
    return m_translationOptions;
}

const QVector<TranslationExample> &OnlineTranslator::examples() const
{
    return m_examples;
}

OnlineTranslator::TranslationError OnlineTranslator::error() const
{
    return m_error;
}

const QString &OnlineTranslator::errorString() const
{
    return m_errorString;
}

bool OnlineTranslator::isSourceTranslitEnabled() const
{
    return m_sourceTranslitEnabled;
}

void OnlineTranslator::setSourceTranslitEnabled(bool enable)
{
    m_sourceTranslitEnabled = enable;
}

bool OnlineTranslator::isTranslationTranslitEnabled() const
{
    return m_translationTranslitEnabled;
}

void OnlineTranslator::setTranslationTranslitEnabled(bool enable)
{
    m_translationTranslitEnabled = enable;
}

bool OnlineTranslator::isSourceTranscriptionEnabled() const
{
    return m_sourceTranscriptionEnabled;
}

void OnlineTranslator::setSourceTranscriptionEnabled(bool enable)
{
    m_sourceTranscriptionEnabled = enable;
}

bool OnlineTranslator::isTranslationOptionsEnabled() const
{
    return m_translationOptionsEnabled;
}

void OnlineTranslator::setTranslationOptionsEnabled(bool enable)
{
    m_translationOptionsEnabled = enable;
}

bool OnlineTranslator::isExamplesEnabled() const
{
    return m_examplesEnabled;
}

void OnlineTranslator::setExamplesEnabled(bool enable)
{
    m_examplesEnabled = enable;
}

const QString &OnlineTranslator::instance()
{
    return m_instance;
}

void OnlineTranslator::setInstance(QString url)
{
    m_instance = qMove(url);
}

QString OnlineTranslator::languageName(Language lang)
{
    switch (lang) {
    case Auto:
        return tr("Automatically detect");
    case Afrikaans:
        return tr("Afrikaans");
    case Albanian:
        return tr("Albanian");
    case Amharic:
        return tr("Amharic");
    case Arabic:
        return tr("Arabic");
    case Armenian:
        return tr("Armenian");
    case Assamese:
        return tr("Assamese");
    case Aymara:
        return tr("Aymara");
    case Azerbaijani:
        return tr("Azerbaijani");
    case Bajan:
        return tr("Bajan");
    case BalkanGipsy:
        return tr("Balkan Gipsy");
    case Bambara:
        return tr("Bambara");
    case Bangla:
        return tr("Bangla");
    case Bashkir:
        return tr("Bashkir");
    case Basque:
        return tr("Basque");
    case Belarusian:
        return tr("Belarusian");
    case Bemba:
        return tr("Bemba");
    case Bhojpuri:
        return tr("Bhojpuri");
    case Bislama:
        return tr("Bislama");
    case Bosnian:
        return tr("Bosnian");
    case Breton:
        return tr("Breton");
    case Bulgarian:
        return tr("Bulgarian");
    case Cantonese:
        return tr("Cantonese (Traditional)");
    case Catalan:
        return tr("Catalan");
    case Cebuano:
        return tr("Cebuano");
    case Chamorro:
        return tr("Chamorro");
    case Chichewa:
        return tr("Chichewa");
    case ChineseLiterary:
        return tr("Chinese (Literary)");
    case ChineseSimplified:
        return tr("Chinese (Simplified)");
    case ChineseTraditional:
        return tr("Chinese (Traditional)");
    case Chuvash:
        return tr("Chuvash");
    case Comorian:
        return tr("Comorian (Ngazidja)");
    case Coptic:
        return tr("Coptic");
    case Corsican:
        return tr("Corsican");
    case AntiguanCreole:
        return tr("Creole English (Antigua and Barbuda)");
    case BahamianCreole:
        return tr("Creole English (Bahamas)");
    case GrenadianCreole:
        return tr("Creole English (Grenadian)");
    case GuyaneseCreole:
        return tr("Creole English (Guyanese)");
    case JamaicanCreole:
        return tr("Creole English (Jamaican)");
    case VincentianCreole:
        return tr("Creole English (Vincentian)");
    case VirginIslandsCreole:
        return tr("Creole English (Virgin Islands)");
    case SaintLucianCreole:
        return tr("Creole French (Saint Lucian)");
    case SeselwaCreole:
        return tr("Creole French (Seselwa)");
    case UpperGuineaCreole:
        return tr("Creole Portuguese (Upper Guinea)");
    case Croatian:
        return tr("Croatian");
    case Czech:
        return tr("Czech");
    case Danish:
        return tr("Danish");
    case Dari:
        return tr("Dari");
    case Divehi:
        return tr("Divehi");
    case Dogri:
        return tr("Dogri");
    case Dutch:
        return tr("Dutch");
    case Dzongkha:
        return tr("Dzongkha");
    case Elvish:
        return tr("Elvish (Sindarin)");
    case Emoji:
        return tr("Emoji");
    case English:
        return tr("English");
    case Esperanto:
        return tr("Esperanto");
    case Estonian:
        return tr("Estonian");
    case Ewe:
        return tr("Ewe");
    case Fanagalo:
        return tr("Fanagalo");
    case Faroese:
        return tr("Faroese");
    case Fijian:
        return tr("Fijian");
    case Filipino:
        return tr("Filipino");
    case Finnish:
        return tr("Finnish");
    case French:
        return tr("French");
    case FrenchCanada:
        return tr("French (Canada)");
    case Frisian:
        return tr("Frisian");
    case Galician:
        return tr("Galician");
    case Ganda:
        return tr("Ganda");
    case Georgian:
        return tr("Georgian");
    case German:
        return tr("German");
    case Greek:
        return tr("Greek");
    case GreekClassical:
        return tr("Greek (Classical)");
    case Guarani:
        return tr("Guarani");
    case Gujarati:
        return tr("Gujarati");
    case HaitianCreole:
        return tr("Haitian Creole");
    case Hausa:
        return tr("Hausa");
    case Hawaiian:
        return tr("Hawaiian");
    case Hebrew:
        return tr("Hebrew");
    case HillMari:
        return tr("Hill Mari");
    case Hindi:
        return tr("Hindi");
    case Hmong:
        return tr("Hmong");
    case HmongDaw:
        return tr("Hmong Daw");
    case Hungarian:
        return tr("Hungarian");
    case Icelandic:
        return tr("Icelandic");
    case Igbo:
        return tr("Igbo");
    case Ilocano:
        return tr("Ilocano");
    case Indonesian:
        return tr("Indonesian");
    case Inuinnaqtun:
        return tr("Inuinnaqtun");
    case Inuktitut:
        return tr("Inuktitut");
    case InuktitutGreenlandic:
        return tr("Inuktitut (Greenlandic)");
    case InuktitutLatin:
        return tr("Inuktitut (Latin)");
    case Irish:
        return tr("Irish");
    case Italian:
        return tr("Italian");
    case Japanese:
        return tr("Japanese");
    case Javanese:
        return tr("Javanese");
    case Kabuverdianu:
        return tr("Kabuverdianu");
    case Kabylian:
        return tr("Kabylian");
    case Kannada:
        return tr("Kannada");
    case Kazakh:
        return tr("Kazakh");
    case KazakhLatin:
        return tr("Kazakh (Latin)");
    case Khmer:
        return tr("Khmer");
    case Kinyarwanda:
        return tr("Kinyarwanda");
    case Kirundi:
        return tr("Kirundi");
    case Klingon:
        return tr("Klingon (Latin)");
    case Konkani:
        return tr("Konkani");
    case Korean:
        return tr("Korean");
    case Krio:
        return tr("Krio");
    case KurdishCentral:
        return tr("Kurdish (Central)");
    case KurdishNorthern:
        return tr("Kurdish (Northern)");
    case KurdishSorani:
        return tr("Kurdish (Sorani)");
    case Kyrgyz:
        return tr("Kyrgyz");
    case Lao:
        return tr("Lao");
    case Latin:
        return tr("Latin");
    case Latvian:
        return tr("Latvian");
    case Lingala:
        return tr("Lingala");
    case Lithuanian:
        return tr("Lithuanian");
    case LowerSorbian:
        return tr("Lower Sorbian");
    case Luganda:
        return tr("Luganda");
    case Luxembourgish:
        return tr("Luxembourgish");
    case Macedonian:
        return tr("Macedonian");
    case Maithili:
        return tr("Maithili");
    case Malagasy:
        return tr("Malagasy");
    case Malay:
        return tr("Malay");
    case Malayalam:
        return tr("Malayalam");
    case Maltese:
        return tr("Maltese");
    case ManxGaelic:
        return tr("Manx Gaelic");
    case Marathi:
        return tr("Marathi");
    case Mari:
        return tr("Mari");
    case Marshallese:
        return tr("Marshallese");
    case Meiteilon:
        return tr("Meiteilon (Manipuri)");
    case Mende:
        return tr("Mende");
    case Mizo:
        return tr("Mizo");
    case Mongolian:
        return tr("Mongolian");
    case MongolianCyrillic:
        return tr("Mongolian (Cyrillic)");
    case MongolianTraditional:
        return tr("Mongolian (Traditional)");
    case Morisyen:
        return tr("Morisyen");
    case Myanmar:
        return tr("Myanmar (Burmese)");
    case Maori:
        return tr("Māori");
    case Nepali:
        return tr("Nepali");
    case Niuean:
        return tr("Niuean");
    case Norwegian:
        return tr("Norwegian");
    case Nyanja:
        return tr("Nyanja");
    case Odia:
        return tr("Odia");
    case Oromo:
        return tr("Oromo");
    case Palauan:
        return tr("Palauan");
    case Papiamentu:
        return tr("Papiamentu");
    case Pashto:
        return tr("Pashto");
    case Persian:
        return tr("Persian");
    case Pijin:
        return tr("Pijin");
    case Polish:
        return tr("Polish");
    case PortugueseBrazilian:
        return tr("Portuguese (Brazilian)");
    case PortuguesePortugal:
        return tr("Portuguese (Portugal)");
    case Potawatomi:
        return tr("Potawatomi");
    case Punjabi:
        return tr("Punjabi");
    case Quechua:
        return tr("Quechua");
    case QueretaroOtomi:
        return tr("Querétaro Otomi");
    case Romanian:
        return tr("Romanian");
    case Rundi:
        return tr("Rundi");
    case Russian:
        return tr("Russian");
    case Samoan:
        return tr("Samoan");
    case Sango:
        return tr("Sango");
    case Sanskrit:
        return tr("Sanskrit");
    case ScotsGaelic:
        return tr("Scots Gaelic");
    case SerbianCyrillic:
        return tr("Serbian (Cyrillic)");
    case SerbianLatin:
        return tr("Serbian (Latin)");
    case Sesotho:
        return tr("Sesotho");
    case SesothoSaLeboa:
        return tr("Sesotho sa Leboa");
    case Setswana:
        return tr("Setswana");
    case Shona:
        return tr("Shona");
    case Sindhi:
        return tr("Sindhi");
    case Sinhala:
        return tr("Sinhala");
    case Slovak:
        return tr("Slovak");
    case Slovenian:
        return tr("Slovenian");
    case Somali:
        return tr("Somali");
    case Spanish:
        return tr("Spanish");
    case SrananTongo:
        return tr("Sranan Tongo");
    case Sundanese:
        return tr("Sundanese");
    case Swahili:
        return tr("Swahili");
    case Swedish:
        return tr("Swedish");
    case Syriac:
        return tr("Syriac (Aramaic)");
    case Tahitian:
        return tr("Tahitian");
    case Tajik:
        return tr("Tajik");
    case Tamashek:
        return tr("Tamashek (Tuareg)");
    case Tamil:
        return tr("Tamil");
    case Tatar:
        return tr("Tatar");
    case Telugu:
        return tr("Telugu");
    case Tetum:
        return tr("Tetum");
    case Thai:
        return tr("Thai");
    case Tibetan:
        return tr("Tibetan");
    case Tigrinya:
        return tr("Tigrinya");
    case TokPisin:
        return tr("Tok Pisin");
    case Tokelauan:
        return tr("Tokelauan");
    case Tongan:
        return tr("Tongan");
    case Tsonga:
        return tr("Tsonga");
    case Turkish:
        return tr("Turkish");
    case Turkmen:
        return tr("Turkmen");
    case Tuvaluan:
        return tr("Tuvaluan");
    case Twi:
        return tr("Twi");
    case Udmurt:
        return tr("Udmurt");
    case Ukrainian:
        return tr("Ukrainian");
    case Uma:
        return tr("Uma");
    case UpperSorbian:
        return tr("Upper Sorbian");
    case Urdu:
        return tr("Urdu");
    case Uyghur:
        return tr("Uyghur");
    case UzbekCyrillic:
        return tr("Uzbek (Cyrillic)");
    case UzbekLatin:
        return tr("Uzbek (Latin)");
    case Vietnamese:
        return tr("Vietnamese");
    case Wallisian:
        return tr("Wallisian");
    case Welsh:
        return tr("Welsh");
    case Wolof:
        return tr("Wolof");
    case Xhosa:
        return tr("Xhosa");
    case Yakut:
        return tr("Yakut");
    case Yiddish:
        return tr("Yiddish");
    case Yoruba:
        return tr("Yoruba");
    case YucatecMaya:
        return tr("Yucatec Maya");
    case Zulu:
        return tr("Zulu");
    default:
        return {};
    }
}

QString OnlineTranslator::languageCode(Language lang)
{
    return s_genericLanguageCodes.value(lang);
}

OnlineTranslator::Language OnlineTranslator::language(const QLocale &locale)
{
    switch (locale.language()) {
    case QLocale::Afrikaans:
        return Afrikaans;
    case QLocale::Albanian:
        return Albanian;
    case QLocale::Amharic:
        return Amharic;
    case QLocale::Arabic:
        return Arabic;
    case QLocale::Armenian:
        return Armenian;
    case QLocale::Assamese:
        return Assamese;
    case QLocale::Aymara:
        return Aymara;
    case QLocale::Azerbaijani:
        return Azerbaijani;
    case QLocale::Bambara:
        return Bambara;
    case QLocale::Bashkir:
        return Bashkir;
    case QLocale::Basque:
        return Basque;
    case QLocale::Belarusian:
        return Belarusian;
    case QLocale::Bemba:
        return Bemba;
    case QLocale::Bhojpuri:
        return Bhojpuri;
    case QLocale::Bislama:
        return Bislama;
    case QLocale::Bosnian:
        return Bosnian;
    case QLocale::Breton:
        return Breton;
    case QLocale::Bulgarian:
        return Bulgarian;
    case QLocale::Cantonese:
        return Cantonese;
    case QLocale::Catalan:
        return Catalan;
    case QLocale::Cebuano:
        return Cebuano;
    case QLocale::Chamorro:
        return Chamorro;
    case QLocale::Chinese:
        return ChineseSimplified;
    case QLocale::Chuvash:
        return Chuvash;
    case QLocale::Coptic:
        return Coptic;
    case QLocale::Corsican:
        return Corsican;
    case QLocale::Croatian:
        return Croatian;
    case QLocale::Czech:
        return Czech;
    case QLocale::Danish:
        return Danish;
    case QLocale::Divehi:
        return Divehi;
    case QLocale::Dogri:
        return Dogri;
    case QLocale::Dutch:
        return Dutch;
    case QLocale::Dzongkha:
        return Dzongkha;
    case QLocale::English:
        return English;
    case QLocale::Esperanto:
        return Esperanto;
    case QLocale::Estonian:
        return Estonian;
    case QLocale::Ewe:
        return Ewe;
    case QLocale::Faroese:
        return Faroese;
    case QLocale::Fijian:
        return Fijian;
    case QLocale::Filipino:
        return Filipino;
    case QLocale::Finnish:
        return Finnish;
    case QLocale::French:
        return French;
    case QLocale::Frisian:
        return Frisian;
    case QLocale::Galician:
        return Galician;
    case QLocale::Ganda:
        return Ganda;
    case QLocale::Georgian:
        return Georgian;
    case QLocale::German:
        return German;
    case QLocale::Greek:
        return Greek;
    case QLocale::Guarani:
        return Guarani;
    case QLocale::Gujarati:
        return Gujarati;
    case QLocale::Hausa:
        return Hausa;
    case QLocale::Hawaiian:
        return Hawaiian;
    case QLocale::Hebrew:
        return Hebrew;
    case QLocale::Hindi:
        return Hindi;
    case QLocale::Hungarian:
        return Hungarian;
    case QLocale::Icelandic:
        return Icelandic;
    case QLocale::Igbo:
        return Igbo;
    case QLocale::Indonesian:
        return Indonesian;
    case QLocale::Inuktitut:
        return Inuktitut;
    case QLocale::Irish:
        return Irish;
    case QLocale::Italian:
        return Italian;
    case QLocale::Japanese:
        return Japanese;
    case QLocale::Javanese:
        return Javanese;
    case QLocale::Kabuverdianu:
        return Kabuverdianu;
    case QLocale::Kannada:
        return Kannada;
    case QLocale::Kazakh:
        return Kazakh;
    case QLocale::Khmer:
        return Khmer;
    case QLocale::Kinyarwanda:
        return Kinyarwanda;
    case QLocale::Konkani:
        return Konkani;
    case QLocale::Korean:
        return Korean;
    case QLocale::Lao:
        return Lao;
    case QLocale::Latin:
        return Latin;
    case QLocale::Latvian:
        return Latvian;
    case QLocale::Lingala:
        return Lingala;
    case QLocale::Lithuanian:
        return Lithuanian;
    case QLocale::LowerSorbian:
        return LowerSorbian;
    case QLocale::Luxembourgish:
        return Luxembourgish;
    case QLocale::Macedonian:
        return Macedonian;
    case QLocale::Maithili:
        return Maithili;
    case QLocale::Malagasy:
        return Malagasy;
    case QLocale::Malay:
        return Malay;
    case QLocale::Malayalam:
        return Malayalam;
    case QLocale::Maltese:
        return Maltese;
    case QLocale::Marathi:
        return Marathi;
    case QLocale::Marshallese:
        return Marshallese;
    case QLocale::Mende:
        return Mende;
    case QLocale::Mongolian:
        return Mongolian;
    case QLocale::Morisyen:
        return Morisyen;
    case QLocale::Maori:
        return Maori;
    case QLocale::Nepali:
        return Nepali;
    case QLocale::NorwegianBokmal:
        return Norwegian;
    case QLocale::Nyanja:
        return Nyanja;
    case QLocale::Oromo:
        return Oromo;
    case QLocale::Palauan:
        return Palauan;
    case QLocale::Pashto:
        return Pashto;
    case QLocale::Persian:
        return Persian;
    case QLocale::Polish:
        return Polish;
    case QLocale::Punjabi:
        return Punjabi;
    case QLocale::Quechua:
        return Quechua;
    case QLocale::Romanian:
        return Romanian;
    case QLocale::Rundi:
        return Rundi;
    case QLocale::Russian:
        return Russian;
    case QLocale::Samoan:
        return Samoan;
    case QLocale::Sango:
        return Sango;
    case QLocale::Sanskrit:
        return Sanskrit;
    case QLocale::Shona:
        return Shona;
    case QLocale::Sindhi:
        return Sindhi;
    case QLocale::Sinhala:
        return Sinhala;
    case QLocale::Slovak:
        return Slovak;
    case QLocale::Slovenian:
        return Slovenian;
    case QLocale::Somali:
        return Somali;
    case QLocale::Spanish:
        return Spanish;
    case QLocale::Sundanese:
        return Sundanese;
    case QLocale::Swahili:
        return Swahili;
    case QLocale::Swedish:
        return Swedish;
    case QLocale::Syriac:
        return Syriac;
    case QLocale::Tahitian:
        return Tahitian;
    case QLocale::Tajik:
        return Tajik;
    case QLocale::Tamil:
        return Tamil;
    case QLocale::Tatar:
        return Tatar;
    case QLocale::Telugu:
        return Telugu;
    case QLocale::Thai:
        return Thai;
    case QLocale::Tibetan:
        return Tibetan;
    case QLocale::Tigrinya:
        return Tigrinya;
    case QLocale::TokPisin:
        return TokPisin;
    case QLocale::Tongan:
        return Tongan;
    case QLocale::Tsonga:
        return Tsonga;
    case QLocale::Turkish:
        return Turkish;
    case QLocale::Turkmen:
        return Turkmen;
    case QLocale::Akan:
        return Twi;
    case QLocale::Ukrainian:
        return Ukrainian;
    case QLocale::UpperSorbian:
        return UpperSorbian;
    case QLocale::Urdu:
        return Urdu;
    case QLocale::Vietnamese:
        return Vietnamese;
    case QLocale::Welsh:
        return Welsh;
    case QLocale::Wolof:
        return Wolof;
    case QLocale::Xhosa:
        return Xhosa;
    case QLocale::Yiddish:
        return Yiddish;
    case QLocale::Yoruba:
        return Yoruba;
    case QLocale::Zulu:
        return Zulu;
    default:
        return English;
    }
}

// Returns general language code
OnlineTranslator::Language OnlineTranslator::language(const QString &langCode)
{
    return s_genericLanguageCodes.key(langCode, NoLanguage);
}

bool OnlineTranslator::isSupportsAutodetection(Engine engine)
{
    switch (engine) {
    case Deepl:
    case Reverso:
        return false;
    case LibreTranslate:
    case Yandex:
    case Google:
    case Duckduckgo:
    case Mymemory:
        return true;
    }

    Q_UNREACHABLE();
}

void OnlineTranslator::skipGarbageText()
{
    m_translation.append(sender()->property(s_textProperty).toString());
}

void OnlineTranslator::requestTranslate()
{
    const QString sourceText = sender()->property(s_textProperty).toString();

    // Generate API url
    QUrl url(m_instance + "/api/translate");
    url.setQuery(QStringLiteral("engine=%1&from=%2&to=%3&text=%4").arg(QString(QMetaEnum::fromType<OnlineTranslator::Engine>().valueToKey(m_engine)).toLower(), languageApiCode(m_engine, m_sourceLang), languageApiCode(m_engine, m_translationLang), QUrl::toPercentEncoding(sourceText)));

    m_currentReply = m_networkManager->get(QNetworkRequest(url));
}

void OnlineTranslator::parseTranslate()
{
    m_currentReply->deleteLater();

    // Check for error
    if (m_currentReply->error() != QNetworkReply::NoError) {
        if (m_currentReply->error() == QNetworkReply::InternalServerError)
            resetData(InstanceError, tr("Instance error: %1").arg(QString(m_currentReply->readAll())));
        else
            resetData(NetworkError, m_currentReply->errorString());
        return;
    }

    // Read Json
    m_jsonResponse = QJsonDocument::fromJson(m_currentReply->readAll());
    const QJsonObject jsonData = m_jsonResponse.object();

    if (m_sourceLang == Auto) {
        // Parse language
        m_sourceLang = language(m_engine, jsonData.value(QStringLiteral("detected")).toString());
        if (m_sourceLang == NoLanguage) {
            resetData(ParsingError, tr("Error: Unable to parse autodetected language"));
            return;
        }
        if (m_onlyDetectLanguage)
            return;
    }

    addSpaceBetweenParts(m_translation);
    addSpaceBetweenParts(m_translationTranslit);
    addSpaceBetweenParts(m_sourceTranslit);

    m_translation.append(jsonData.value(QStringLiteral("translated-text")).toString());

    if (m_translationTranslitEnabled)
        m_translationTranslit.append(jsonData.value(QStringLiteral("target_transliteration")).toString());

    if (m_sourceTranslitEnabled)
        m_sourceTranslit.append(jsonData.value(QStringLiteral("source_transliteration")).toString());

    // Translation options
    if (m_translationOptionsEnabled) {
        const QJsonObject translationOptionsObject = jsonData.value(QStringLiteral("target_equivalent_source_lang")).toObject();
        for (const QString &word : translationOptionsObject.keys()) {
            const QJsonArray translationsArray = translationOptionsObject.value(word).toArray();

            QStringList translations;
            translations.reserve(translationsArray.size());
            for (const QJsonValue &wordTranslation : translationsArray)
                translations.append(wordTranslation.toString());
            m_translationOptions.append({word, translations});
        }
    }

    // Examples
    if (m_examplesEnabled) {
        for (const QJsonValueRef examplesData : jsonData.value(QStringLiteral("word_choices")).toArray()) {
            const QJsonObject exampleObject = examplesData.toObject();
            const QString word = exampleObject.value(QStringLiteral("word")).toString();
            const QString example = exampleObject.value(QStringLiteral("example")).toString();
            const QString definition = exampleObject.value(QStringLiteral("definition")).toString();

            QStringList examplesSource;
            for (const QJsonValue &value : exampleObject.value(QStringLiteral("examples_source")).toArray()) {
                examplesSource.append(value.toString());
            }

            QStringList examplesTarget;
            for (const QJsonValue &value : exampleObject.value(QStringLiteral("examples_target")).toArray()) {
                examplesTarget.append(value.toString());
            }

            m_examples.append({word, example, definition, examplesSource, examplesTarget});
        }
    }
}

void OnlineTranslator::buildStateMachine()
{
    // States (sends a request that will be splitted into several by the translation limit)
    auto *translationState = new QState(m_stateMachine);
    auto *finalState = new QFinalState(m_stateMachine);
    m_stateMachine->setInitialState(translationState);

    translationState->addTransition(translationState, &QState::finished, finalState);

    // Setup translation state
    buildSplitNetworkRequest(translationState, &OnlineTranslator::requestTranslate, &OnlineTranslator::parseTranslate, m_source, s_textLimits);
}

void OnlineTranslator::buildDetectStateMachine()
{
    // States
    auto *detectState = new QState(m_stateMachine);
    auto *finalState = new QFinalState(m_stateMachine);
    m_stateMachine->setInitialState(detectState);

    detectState->addTransition(detectState, &QState::finished, finalState);

    // Setup detect state
    buildNetworkRequestState(detectState, &OnlineTranslator::requestTranslate, &OnlineTranslator::parseTranslate, m_source);
}

void OnlineTranslator::buildSplitNetworkRequest(QState *parent, void (OnlineTranslator::*requestMethod)(), void (OnlineTranslator::*parseMethod)(), const QString &text, int textLimit)
{
    QString unsendedText = text;
    auto *nextTranslationState = new QState(parent);
    parent->setInitialState(nextTranslationState);

    while (!unsendedText.isEmpty()) {
        auto *currentTranslationState = nextTranslationState;
        nextTranslationState = new QState(parent);

        // Do not translate the part if it looks like garbage
        const int splitIndex = getSplitIndex(unsendedText, textLimit);
        if (splitIndex == -1) {
            currentTranslationState->setProperty(s_textProperty, unsendedText.left(textLimit));
            currentTranslationState->addTransition(nextTranslationState);
            connect(currentTranslationState, &QState::entered, this, &OnlineTranslator::skipGarbageText);

            // Remove the parsed part from the next parsing
            unsendedText = unsendedText.mid(textLimit);
        } else {
            buildNetworkRequestState(currentTranslationState, requestMethod, parseMethod, unsendedText.left(splitIndex));
            currentTranslationState->addTransition(currentTranslationState, &QState::finished, nextTranslationState);

            // Remove the parsed part from the next parsing
            unsendedText = unsendedText.mid(splitIndex);
        }
    }

    nextTranslationState->addTransition(new QFinalState(parent));
}

void OnlineTranslator::buildNetworkRequestState(QState *parent, void (OnlineTranslator::*requestMethod)(), void (OnlineTranslator::*parseMethod)(), const QString &text)
{
    // Network substates
    auto *requestingState = new QState(parent);
    auto *parsingState = new QState(parent);

    parent->setInitialState(requestingState);

    // Substates transitions
    requestingState->addTransition(m_networkManager, &QNetworkAccessManager::finished, parsingState);
    parsingState->addTransition(new QFinalState(parent));

    // Setup requesting state
    requestingState->setProperty(s_textProperty, text);
    connect(requestingState, &QState::entered, this, requestMethod);

    // Setup parsing state
    connect(parsingState, &QState::entered, this, parseMethod);
}

void OnlineTranslator::resetData(TranslationError error, const QString &errorString)
{
    m_error = error;
    m_errorString = errorString;
    m_translation.clear();
    m_translationTranslit.clear();
    m_sourceTranslit.clear();
    m_sourceTranscription.clear();
    m_translationOptions.clear();
    m_examples.clear();

    m_stateMachine->stop();
    for (QAbstractState *state : m_stateMachine->findChildren<QAbstractState *>()) {
        if (!m_stateMachine->configuration().contains(state))
            state->deleteLater();
    }
}

// Returns engine-specific language code for translation
QString OnlineTranslator::languageApiCode(Engine engine, Language lang)
{
    switch (engine) {
    case Google:
        return s_googleLanguageCodes.value(lang, s_genericLanguageCodes.value(lang));
    case Yandex:
        return s_yandexLanguageCodes.value(lang, s_genericLanguageCodes.value(lang));
    case Duckduckgo:
        return s_ddgLanguageCodes.value(lang, s_genericLanguageCodes.value(lang));
    case Reverso:
        return s_reverso.value(lang, s_genericLanguageCodes.value(lang));
    case Mymemory:
        return s_mymemory.value(lang, s_genericLanguageCodes.value(lang));
    case Deepl:
    case LibreTranslate:
        return s_genericLanguageCodes.value(lang);
    }

    Q_UNREACHABLE();
}

// Parse language from response language code
OnlineTranslator::Language OnlineTranslator::language(Engine engine, const QString &langCode)
{
    switch (engine) {
    case Google:
        return s_googleLanguageCodes.key(langCode, s_genericLanguageCodes.key(langCode, NoLanguage));
    case Yandex:
        return s_yandexLanguageCodes.key(langCode, s_genericLanguageCodes.key(langCode, NoLanguage));
    case Duckduckgo:
        return s_ddgLanguageCodes.key(langCode, s_genericLanguageCodes.key(langCode, NoLanguage));
    case Deepl:
    case LibreTranslate:
    case Mymemory:
    case Reverso:
        return s_genericLanguageCodes.key(langCode, NoLanguage);
    }

    Q_UNREACHABLE();
}

// Get split index of the text according to the limit
int OnlineTranslator::getSplitIndex(const QString &untranslatedText, int limit)
{
    if (untranslatedText.size() < limit)
        return limit;

    int splitIndex = untranslatedText.lastIndexOf(QLatin1String(". "), limit - 1);
    if (splitIndex != -1)
        return splitIndex + 1;

    splitIndex = untranslatedText.lastIndexOf(' ', limit - 1);
    if (splitIndex != -1)
        return splitIndex + 1;

    splitIndex = untranslatedText.lastIndexOf('\n', limit - 1);
    if (splitIndex != -1)
        return splitIndex + 1;

    // Non-breaking space
    splitIndex = untranslatedText.lastIndexOf(0x00a0, limit - 1);
    if (splitIndex != -1)
        return splitIndex + 1;

    // If the text has not passed any check and is most likely garbage
    return limit;
}

bool OnlineTranslator::isContainsSpace(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](QChar symbol) {
        return symbol.isSpace();
    });
}

void OnlineTranslator::addSpaceBetweenParts(QString &text)
{
    if (text.isEmpty())
        return;

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    if (!text.back().isSpace()) {
#else
    if (!text.at(text.size() - 1).isSpace()) {
#endif
        text.append(' ');
    }
}
