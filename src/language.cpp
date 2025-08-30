/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "language.h"

#include <QDebug>

// Static member initialization
QHash<QString, Language::CustomLanguageData> Language::s_customLanguages;
QHash<QString, QString> Language::s_customByIso639_1;
QHash<QString, QString> Language::s_customByIso639_2;
QHash<QString, QString> Language::s_customByName;
int Language::s_nextCustomId = 1;
Language::CustomLanguageRegistryChangedCallback Language::s_registryChangedCallback = nullptr;

Language::Language()
    : Language(QLocale::c())
{
}

Language::Language(const QLocale &locale)
    : m_source(QLocaleSource)
    , m_locale(locale)
    , m_customId(-1)
{
}

Language::Language(const QString &code)
    : m_source(QLocaleSource)
    , m_locale(QLocale::c())
    , m_customId(-1)
{
    // Try to find as custom language first
    if (s_customLanguages.contains(code)) {
        const auto &data = s_customLanguages[code];
        m_source = CustomSource;
        m_customId = data.id;
        m_customCode = data.code;
        m_customName = data.name;
        m_customIso639_1 = data.iso639_1;
        m_customIso639_2 = data.iso639_2;
        return;
    }

    // Try ISO639-1/2 lookup in custom languages
    if (s_customByIso639_1.contains(code) || s_customByIso639_2.contains(code)) {
        const QString &customCode = s_customByIso639_1.value(code, s_customByIso639_2.value(code));
        const auto &data = s_customLanguages[customCode];
        m_source = CustomSource;
        m_customId = data.id;
        m_customCode = data.code;
        m_customName = data.name;
        m_customIso639_1 = data.iso639_1;
        m_customIso639_2 = data.iso639_2;
        return;
    }

    // Use QLocale's built-in code conversion
    QLocale::Language lang = QLocale::codeToLanguage(code, QLocale::AnyLanguageCode);
    if (lang != QLocale::AnyLanguage) {
        m_locale = QLocale(lang);
        return;
    }

    // Try as full locale string (e.g., "en_US", "zh_CN")
    QLocale locale(code);
    if (locale != QLocale::c()) {
        m_locale = locale;
        return;
    }

    // If nothing worked, remain as invalid (QLocale::c())
}

Language::Language(const Language &other)
    : m_source(other.m_source)
    , m_locale(other.m_locale)
    , m_customCode(other.m_customCode)
    , m_customName(other.m_customName)
    , m_customIso639_1(other.m_customIso639_1)
    , m_customIso639_2(other.m_customIso639_2)
    , m_customId(other.m_customId)
{
}

Language::Language(int customId, const QString &code, const QString &name,
                   const QString &iso639_1, const QString &iso639_2)
    : m_source(CustomSource)
    , m_locale(QLocale::c())
    , m_customCode(code)
    , m_customName(name)
    , m_customIso639_1(iso639_1)
    , m_customIso639_2(iso639_2)
    , m_customId(customId)
{
}

Language &Language::operator=(const Language &other)
{
    if (this != &other) {
        m_source = other.m_source;
        m_locale = other.m_locale;
        m_customCode = other.m_customCode;
        m_customName = other.m_customName;
        m_customIso639_1 = other.m_customIso639_1;
        m_customIso639_2 = other.m_customIso639_2;
        m_customId = other.m_customId;
    }
    return *this;
}

Language &Language::operator=(const QLocale &locale)
{
    m_source = QLocaleSource;
    m_locale = locale;
    m_customCode.clear();
    m_customName.clear();
    m_customIso639_1.clear();
    m_customIso639_2.clear();
    m_customId = -1;
    return *this;
}

bool Language::operator==(const Language &other) const
{
    if (m_source != other.m_source) {
        return false;
    }

    if (m_source == QLocaleSource) {
        return m_locale == other.m_locale;
    } else {
        return m_customId == other.m_customId;
    }
}

bool Language::operator!=(const Language &other) const
{
    return !(*this == other);
}

Language::Source Language::source() const
{
    return m_source;
}

bool Language::isValid() const
{
    if (m_source == QLocaleSource) {
        return m_locale != QLocale::c();
    } else {
        return m_customId >= 0 && !m_customCode.isEmpty();
    }
}

bool Language::isEmpty() const
{
    return !isValid();
}

bool Language::hasQLocaleEquivalent() const
{
    return m_source == QLocaleSource && isValid();
}

QLocale Language::toQLocale() const
{
    if (m_source == QLocaleSource) {
        return m_locale;
    }

    // For custom languages, return C locale as fallback
    return QLocale::c();
}

QString Language::toIso639_1() const
{
    if (m_source == QLocaleSource && isValid()) {
        // Use QLocale's built-in method
        return QLocale::languageToCode(m_locale.language(), QLocale::ISO639Part1);
    } else if (m_source == CustomSource) {
        return m_customIso639_1;
    }

    return QString();
}

QString Language::toIso639_2() const
{
    if (m_source == QLocaleSource && isValid()) {
        // Use QLocale's built-in method (prefer bibliographic codes)
        QString code = QLocale::languageToCode(m_locale.language(), QLocale::ISO639Part2B);
        if (code.isEmpty()) {
            // Fallback to terminological codes
            code = QLocale::languageToCode(m_locale.language(), QLocale::ISO639Part2T);
        }
        return code;
    } else if (m_source == CustomSource) {
        return m_customIso639_2;
    }

    return QString();
}

QString Language::toCode() const
{
    // For custom languages, prefer the full custom code to preserve script/territory information
    if (m_source == CustomSource) {
        return m_customCode;
    }

    // For QLocale languages, prefer ISO639-1, then ISO639-2, then locale name
    QString iso1 = toIso639_1();
    if (!iso1.isEmpty()) {
        return iso1;
    }

    QString iso2 = toIso639_2();
    if (!iso2.isEmpty()) {
        return iso2;
    }

    if (m_source == QLocaleSource && isValid()) {
        return m_locale.name();
    }

    return QString();
}

QString Language::toString() const
{
    return toCode();
}

QString Language::name() const
{
    if (m_source == QLocaleSource && isValid()) {
        return QLocale::languageToString(m_locale.language());
    } else if (m_source == CustomSource) {
        return m_customName;
    }

    return QString();
}

QString Language::nativeName() const
{
    if (m_source == QLocaleSource && isValid()) {
        return m_locale.nativeLanguageName();
    } else if (m_source == CustomSource) {
        // Custom languages don't have native names by default
        return m_customName;
    }

    return QString();
}

QString Language::displayName() const
{
    QString baseName = name();

    // Always show the most complete code available to ensure uniqueness
    QString fullCode;
    if (hasQLocaleEquivalent()) {
        // Use full BCP47 name for QLocale-backed languages (includes territory + script)
        fullCode = toQLocale().bcp47Name();
    } else {
        // For custom languages, use the registered code (most complete available)
        fullCode = toCode();
    }

    if (!fullCode.isEmpty()) {
        return QString("%1 (%2)").arg(baseName, fullCode);
    }

    return baseName;
}

QString Language::customCode() const
{
    return m_customCode;
}

QString Language::customName() const
{
    return m_customName;
}

// Static factory methods
Language Language::fromIso639_1(const QString &code)
{
    QLocale::Language lang = QLocale::codeToLanguage(code, QLocale::ISO639Part1);
    if (lang != QLocale::AnyLanguage) {
        return Language(QLocale(lang));
    }

    // Try custom languages
    return Language(code);
}

Language Language::fromIso639_2(const QString &code)
{
    QLocale::Language lang = QLocale::codeToLanguage(code, QLocale::ISO639Part2B | QLocale::ISO639Part2T);
    if (lang != QLocale::AnyLanguage) {
        return Language(QLocale(lang));
    }

    // Try custom languages
    return Language(code);
}

Language Language::fromString(const QString &name)
{
    // Try custom languages first
    if (s_customByName.contains(name)) {
        const QString &customCode = s_customByName[name];
        return Language(customCode);
    }

    // Try QLocale languages by iterating through available locales
    auto allLocales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory);
    for (const auto &locale : allLocales) {
        if (QLocale::languageToString(locale.language()) == name) {
            return Language(locale);
        }
    }

    return Language(); // Invalid language
}

Language Language::registerCustomLanguage(const QString &code, const QString &name,
                                          const QString &iso639_1, const QString &iso639_2)
{
    if (code.isEmpty() || name.isEmpty()) {
        qWarning() << "Language::registerCustomLanguage: code and name cannot be empty";
        return Language();
    }

    if (s_customLanguages.contains(code)) {
        qDebug() << "Language::registerCustomLanguage: language" << code << "already registered";
        return Language(code);
    }

    int id = s_nextCustomId++;
    CustomLanguageData data;
    data.id = id;
    data.code = code;
    data.name = name;
    data.iso639_1 = iso639_1;
    data.iso639_2 = iso639_2;

    s_customLanguages[code] = data;

    if (!iso639_1.isEmpty()) {
        s_customByIso639_1[iso639_1] = code;
    }

    if (!iso639_2.isEmpty()) {
        s_customByIso639_2[iso639_2] = code;
    }

    s_customByName[name] = code;

    qDebug() << "Language::registerCustomLanguage: registered" << code << "(" << name << ")";

    // Notify about registry change (for persistence)
    if (s_registryChangedCallback) {
        s_registryChangedCallback();
    }

    return Language(id, code, name, iso639_1, iso639_2);
}

QList<Language> Language::allLanguages()
{
    QList<Language> result = qlocaleLanguages();
    result.append(customLanguages());
    return result;
}

QList<Language> Language::qlocaleLanguages()
{
    QList<Language> result;
    auto allLocales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory);

    for (const auto &locale : allLocales) {
        if (locale != QLocale::c()) {
            result.append(Language(locale));
        }
    }

    return result;
}

QList<Language> Language::customLanguages()
{
    QList<Language> result;
    for (const auto &data : s_customLanguages) {
        result.append(Language(data.id, data.code, data.name, data.iso639_1, data.iso639_2));
    }

    return result;
}

const QHash<QString, Language::CustomLanguageData> &Language::getCustomLanguages()
{
    return s_customLanguages;
}

void Language::loadCustomLanguagesFromSettings()
{
    // This method is called by the application at startup to load persisted custom languages
    // Note: We avoid circular dependency by not including AppSettings here
    // Instead, the application should call AppSettings::loadCustomLanguageRegistry() at startup
}

void Language::setCustomLanguageRegistryChangedCallback(CustomLanguageRegistryChangedCallback callback)
{
    s_registryChangedCallback = callback;
}

Language Language::systemLanguage()
{
    return Language(QLocale::system());
}

Language Language::autoLanguage()
{
    return Language(QLocale::c());
}
