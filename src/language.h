/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <QHash>
#include <QList>
#include <QLocale>
#include <QMetaType>
#include <QString>

/**
 * @brief Extended language representation that wraps QLocale and allows custom extensions
 *
 * The Language class provides a unified interface for language handling that:
 * 1. Uses QLocale as the primary source for standard languages (including Language+Script+Territory combinations)
 * 2. Allows translation providers to register custom languages they support but QLocale doesn't
 * 3. Provides ISO639 code support for interoperability
 * 4. Maintains full compatibility with existing QLocale usage
 *
 * Providers like MozhiTranslationProvider can register languages from OnlineTranslator
 * that aren't representable in QLocale, making the system extensible without hardcoding assumptions.
 */
class Language
{
public:
    /**
     * @brief Language source type
     */
    enum Source {
        QLocaleSource, // Standard QLocale language
        CustomSource // Custom language registered by a provider
    };

    // Constructors
    Language();
    Language(const QLocale &locale);
    Language(const QString &code); // ISO639-1, ISO639-2, or custom code
    Language(const Language &other);

    // Assignment
    Language &operator=(const Language &other);
    Language &operator=(const QLocale &locale);

    // Comparison
    bool operator==(const Language &other) const;
    bool operator!=(const Language &other) const;

    // Core properties
    Source source() const;
    bool isValid() const;
    bool isEmpty() const;

    // QLocale integration
    bool hasQLocaleEquivalent() const;
    QLocale toQLocale() const;

    // String representations
    QString toIso639_1() const; // Two-letter code (en, de, fr)
    QString toIso639_2() const; // Three-letter code (eng, deu, fra)
    QString toCode() const; // Best available code (ISO639-1 > ISO639-2 > custom)
    QString toString() const; // Alias for toCode() for compatibility
    QString name() const; // Human readable English name
    QString nativeName() const; // Native language name when available
    QString displayName() const; // Full display name with BCP47 code for UI

    // Custom language properties
    QString customCode() const; // Custom provider code
    QString customName() const; // Custom provider name

    // Static factory methods
    static Language fromIso639_1(const QString &code);
    static Language fromIso639_2(const QString &code);
    static Language fromString(const QString &name);

    // Data structure for custom languages
    struct CustomLanguageData {
        int id;
        QString code;
        QString name;
        QString iso639_1;
        QString iso639_2;
    };

    // Custom language registration (for providers)
    static Language registerCustomLanguage(const QString &code,
                                           const QString &name,
                                           const QString &iso639_1 = QString(),
                                           const QString &iso639_2 = QString());

    // Language enumeration
    static QList<Language> allLanguages();
    static QList<Language> qlocaleLanguages();
    static QList<Language> customLanguages();

    // Custom language registry access (for persistence)
    static const QHash<QString, CustomLanguageData> &getCustomLanguages();
    static void loadCustomLanguagesFromSettings();

    // Callback for saving custom languages when they are registered
    typedef void (*CustomLanguageRegistryChangedCallback)();
    static void setCustomLanguageRegistryChangedCallback(CustomLanguageRegistryChangedCallback callback);

    // Utility methods
    static Language systemLanguage();
    static Language autoLanguage();

private:
    Source m_source;
    QLocale m_locale; // For QLocale-based languages
    QString m_customCode; // For custom languages
    QString m_customName; // For custom languages
    QString m_customIso639_1; // For custom languages
    QString m_customIso639_2; // For custom languages
    int m_customId; // For custom languages

    // Private constructor for custom languages
    Language(int customId, const QString &code, const QString &name,
             const QString &iso639_1, const QString &iso639_2);

    // Static data for custom languages
    static QHash<QString, CustomLanguageData> s_customLanguages; // code -> data
    static QHash<QString, QString> s_customByIso639_1; // iso639_1 -> code
    static QHash<QString, QString> s_customByIso639_2; // iso639_2 -> code
    static QHash<QString, QString> s_customByName; // name -> code
    static int s_nextCustomId;
    static CustomLanguageRegistryChangedCallback s_registryChangedCallback;
};

Q_DECLARE_METATYPE(Language)
Q_DECLARE_METATYPE(Language::Source)

// Qt hash function for Language
inline uint qHash(const Language &language, uint seed = 0)
{
    if (language.hasQLocaleEquivalent()) {
        return ::qHash(language.toQLocale(), static_cast<size_t>(seed));
    } else {
        return ::qHash(language.toCode(), static_cast<size_t>(seed));
    }
}

#endif // LANGUAGE_H