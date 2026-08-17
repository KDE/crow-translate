/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2026 Oleksandr Mikriukov <ur3ley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "appsettings.h"

#include "cmake.h"
#include "languagebuttonswidget.h"
#include "trayicon.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QHotkey>
#include <QKeySequence>
#include <QLibraryInfo>
#include <QMetaEnum>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTranslator>
#include <QVariant>

#include <cstdint>
#include <type_traits>

QTranslator AppSettings::s_appTranslator;
QTranslator AppSettings::s_qtTranslator;

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
#ifndef WITH_PORTABLE_MODE
    , m_settings(new QSettings(this))
#else
    , m_settings(QFile::exists(AppSettings::portableConfigName()) ? new QSettings(AppSettings::portableConfigName(), QSettings::IniFormat, this) : new QSettings(this))
#endif
{
}

void AppSettings::setupLocalization() const
{
    applyLocale(locale());
}

QLocale AppSettings::locale() const
{
    return m_settings->value(QStringLiteral("Locale"), defaultLocale()).value<QLocale>();
}

void AppSettings::setLocale(const QLocale &locale)
{
    if (locale != this->locale()) {
        m_settings->setValue(QStringLiteral("Locale"), locale);
        applyLocale(locale);
    }
}

void AppSettings::applyLocale(const QLocale &locale)
{
    const QLocale newLocale = locale == defaultLocale() ? QLocale::system() : locale;
    QLocale::setDefault(newLocale);
    if (!loadLocale(newLocale.name())) {
        if (!loadLocale(newLocale.bcp47Name())) {
            const int index = newLocale.name().indexOf(QLatin1Char('_'));
            if (index == -1 || !loadLocale(newLocale.name().left(index))) {
                QCoreApplication::removeTranslator(&s_appTranslator);
            }
        }
    }

    if (s_qtTranslator.load(newLocale, QStringLiteral("qt"), QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(&s_qtTranslator);
    } else {
        QCoreApplication::removeTranslator(&s_qtTranslator);
    }
}

// Code adapted from ECM QM loader.
// We use our own implementation instead of automatic loading to let users set locale inside the application.
bool AppSettings::loadLocale(const QString &localeDirName)
{
    const QString subPath = QStringLiteral("locale/%1/LC_MESSAGES/%2_qt.qm").arg(localeDirName).arg(PROJECT_NAME);

    const QString fullPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, subPath);
    if (fullPath.isEmpty()) {
        return false;
    }

    if (!s_appTranslator.load(fullPath)) {
        return false;
    }

    QCoreApplication::installTranslator(&s_appTranslator);
    return true;
}

QLocale AppSettings::defaultLocale()
{
    // We never apply "C" locale and just use it as a special value for <System language>
    // We can't use QLocale::system() because it will be indistinguishable from QLocale constructed for this language
    return QLocale::c();
}

Qt::ScreenOrientation AppSettings::mainWindowOrientation() const
{
    return m_settings->value(QStringLiteral("MainWindowOrientation"), defaultMainWindowOrientation()).value<Qt::ScreenOrientation>();
}

void AppSettings::setMainWindowOrientation(Qt::ScreenOrientation mode)
{
    m_settings->setValue(QStringLiteral("MainWindowOrientation"), mode);
}

Qt::ScreenOrientation AppSettings::defaultMainWindowOrientation()
{
    return Qt::PrimaryOrientation;
}

AppSettings::WindowMode AppSettings::windowMode() const
{
    return m_settings->value(QStringLiteral("WindowMode"), defaultWindowMode()).value<WindowMode>();
}

void AppSettings::setWindowMode(WindowMode mode)
{
    m_settings->setValue(QStringLiteral("WindowMode"), mode);
}

AppSettings::WindowMode AppSettings::defaultWindowMode()
{
    return PopupWindow;
}

int AppSettings::translationNotificationTimeout() const
{
    return m_settings->value(QStringLiteral("Interface/TranslationNotificationTimeout"), defaultTranslationNotificationTimeout()).toInt();
}

void AppSettings::setTranslationNotificationTimeout(int timeout)
{
    m_settings->setValue(QStringLiteral("Interface/TranslationNotificationTimeout"), timeout);
}

int AppSettings::defaultTranslationNotificationTimeout()
{
    return 3;
}

int AppSettings::popupWindowTimeout() const
{
    return m_settings->value(QStringLiteral("Interface/PopupWindowTimeout"), defaultPopupWindowTimeout()).toInt();
}

void AppSettings::setPopupWindowTimeout(int timeout)
{
    m_settings->setValue(QStringLiteral("Interface/PopupWindowTimeout"), timeout);
}

int AppSettings::defaultPopupWindowTimeout()
{
    return 0;
}

bool AppSettings::isShowTrayIcon() const
{
    return m_settings->value(QStringLiteral("TrayIconVisible"), defaultShowTrayIcon()).toBool();
}

void AppSettings::setShowTrayIcon(bool visible)
{
    m_settings->setValue(QStringLiteral("TrayIconVisible"), visible);
}

bool AppSettings::defaultShowTrayIcon()
{
#ifdef Q_OS_LINUX
    // Plasma Mobile currently says that system tray is available by mistake https://invent.kde.org/plasma/plasma-nano/-/issues/1
    if (const QByteArray plasmaPlatform = qgetenv("PLASMA_PLATFORM"); plasmaPlatform.contains("phone"))
        return false;
#endif
    return QSystemTrayIcon::isSystemTrayAvailable();
}

bool AppSettings::isStartMinimized() const
{
    return m_settings->value(QStringLiteral("StartMinimized"), defaultStartMinimized()).toBool();
}

void AppSettings::setStartMinimized(bool minimized)
{
    m_settings->setValue(QStringLiteral("StartMinimized"), minimized);
}

bool AppSettings::defaultStartMinimized()
{
    return false;
}

bool AppSettings::isAutostartEnabled() const
{
    return m_settings->value(QStringLiteral("AutostartEnabled"), defaultAutostartEnabled()).toBool();
}

void AppSettings::setAutostartEnabled(bool enabled)
{
    m_settings->setValue(QStringLiteral("AutostartEnabled"), enabled);
}

bool AppSettings::defaultAutostartEnabled()
{
    return false;
}

#ifdef WITH_PORTABLE_MODE
bool AppSettings::isPortableModeEnabled() const
{
    return m_settings->format() == QSettings::IniFormat;
}

void AppSettings::setPortableModeEnabled(bool enabled)
{
    if (enabled) {
        QFile configFile(AppSettings::portableConfigName());
        (void)configFile.open(QIODevice::NewOnly);
    } else {
        QFile::remove(AppSettings::portableConfigName());
    }
}

QString AppSettings::portableConfigName()
{
    // Initialize lazily because `QCoreApplication::applicationDirPath()` should be called after app creation
    static const QString portableConfigName = QCoreApplication::applicationDirPath() + "/" + QStringLiteral("settings.ini");
    return portableConfigName;
}
#endif

QFont AppSettings::font() const
{
    return m_settings->value(QStringLiteral("Interface/Font"), QGuiApplication::font()).value<QFont>();
}

void AppSettings::setFont(const QFont &font)
{
    m_settings->setValue(QStringLiteral("Interface/Font"), font);
}

double AppSettings::popupOpacity() const
{
    return m_settings->value(QStringLiteral("Interface/PopupOpacity"), defaultPopupOpacity()).toDouble();
}

void AppSettings::setPopupOpacity(double opacity)
{
    m_settings->setValue(QStringLiteral("Interface/PopupOpacity"), opacity);
}

double AppSettings::defaultPopupOpacity()
{
    return 0.8;
}

int AppSettings::popupHeight() const
{
    return m_settings->value(QStringLiteral("Interface/PopupHeight"), defaultPopupHeight()).toInt();
}

void AppSettings::setPopupHeight(int height)
{
    m_settings->setValue(QStringLiteral("Interface/PopupHeight"), height);
}

int AppSettings::defaultPopupHeight()
{
    return 300;
}

int AppSettings::popupWidth() const
{
    return m_settings->value(QStringLiteral("Interface/PopupWidth"), defaultPopupWidth()).toInt();
}

void AppSettings::setPopupWidth(int width)
{
    m_settings->setValue(QStringLiteral("Interface/PopupWidth"), width);
}

int AppSettings::defaultPopupWidth()
{
    return 350;
}

AppSettings::LanguageFormat AppSettings::popupLanguageFormat() const
{
    return m_settings->value(QStringLiteral("Interface/PopupLanguageFormat"), defaultPopupLanguageFormat()).value<LanguageFormat>();
}

void AppSettings::setPopupLanguageFormat(LanguageFormat style)
{
    m_settings->setValue(QStringLiteral("Interface/PopupLanguageFormat"), style);
}

AppSettings::LanguageFormat AppSettings::defaultPopupLanguageFormat()
{
    return IsoCode;
}

AppSettings::LanguageFormat AppSettings::mainWindowLanguageFormat() const
{
    return m_settings->value(QStringLiteral("Interface/MainWindowLanguageFormat"), defaultMainWindowLanguageFormat()).value<LanguageFormat>();
}

void AppSettings::setMainWindowLanguageFormat(LanguageFormat style)
{
    m_settings->setValue(QStringLiteral("Interface/MainWindowLanguageFormat"), style);
}

AppSettings::LanguageFormat AppSettings::defaultMainWindowLanguageFormat()
{
    return FullName;
}

AppSettings::IconType AppSettings::trayIconType() const
{
    return m_settings->value(QStringLiteral("Interface/TrayIconName"), defaultTrayIconType()).value<IconType>();
}

void AppSettings::setTrayIconType(IconType type)
{
    m_settings->setValue(QStringLiteral("Interface/TrayIconName"), type);
}

AppSettings::IconType AppSettings::defaultTrayIconType()
{
    return DefaultIcon;
}

QString AppSettings::customIconPath() const
{
    return m_settings->value(QStringLiteral("Interface/CustomIconPath"), defaultCustomIconPath()).toString();
}

void AppSettings::setCustomIconPath(const QString &path)
{
    m_settings->setValue(QStringLiteral("Interface/CustomIconPath"), path);
}

QString AppSettings::defaultCustomIconPath()
{
    return TrayIcon::trayIconName(AppSettings::DefaultIcon);
}

bool AppSettings::isShowStatusBar() const
{
    return m_settings->value(QStringLiteral("Interface/ShowStatusBar"), defaultShowStatusBar()).toBool();
}

void AppSettings::setShowStatusBar(bool visible)
{
    m_settings->setValue(QStringLiteral("Interface/ShowStatusBar"), visible);
}

bool AppSettings::defaultShowStatusBar()
{
    return true;
}

bool AppSettings::isSourceTranslitEnabled() const
{
    return m_settings->value(QStringLiteral("Translation/SourceTranslitEnabled"), defaultSourceTranslitEnabled()).toBool();
}

void AppSettings::setSourceTranslitEnabled(bool enable)
{
    m_settings->setValue(QStringLiteral("Translation/SourceTranslitEnabled"), enable);
}

bool AppSettings::defaultSourceTranslitEnabled()
{
    return false;
}

bool AppSettings::isTranslationTranslitEnabled() const
{
    return m_settings->value(QStringLiteral("Translation/TranslationTranslitEnabled"), defaultTranslationTranslitEnabled()).toBool();
}

void AppSettings::setTranslationTranslitEnabled(bool enable)
{
    m_settings->setValue(QStringLiteral("Translation/TranslationTranslitEnabled"), enable);
}

bool AppSettings::defaultTranslationTranslitEnabled()
{
    return false;
}

bool AppSettings::isSourceTranscriptionEnabled() const
{
    return m_settings->value(QStringLiteral("Translation/SourceTranscriptionEnabled"), defaultSourceTranscriptionEnabled()).toBool();
}

void AppSettings::setSourceTranscriptionEnabled(bool enable)
{
    m_settings->setValue(QStringLiteral("Translation/SourceTranscriptionEnabled"), enable);
}

bool AppSettings::defaultSourceTranscriptionEnabled()
{
    return true;
}

bool AppSettings::isTranslationOptionsEnabled() const
{
    return m_settings->value(QStringLiteral("Translation/TranslationOptionsEnabled"), defaultTranslationOptionsEnabled()).toBool();
}

void AppSettings::setTranslationOptionsEnabled(bool enable)
{
    m_settings->setValue(QStringLiteral("Translation/TranslationOptionsEnabled"), enable);
}

bool AppSettings::defaultTranslationOptionsEnabled()
{
    return true;
}

bool AppSettings::isExamplesEnabled() const
{
    return m_settings->value(QStringLiteral("Translation/ExamplesEnabled"), defaultExamplesEnabled()).toBool();
}

void AppSettings::setExamplesEnabled(bool enable)
{
    m_settings->setValue(QStringLiteral("Translation/ExamplesEnabled"), enable);
}

bool AppSettings::defaultExamplesEnabled()
{
    return true;
}

bool AppSettings::isSimplifySource() const
{
    return m_settings->value(QStringLiteral("Translation/SimplifySource"), defaultSimplifySource()).toBool();
}

void AppSettings::setSimplifySource(bool simplify)
{
    m_settings->setValue(QStringLiteral("Translation/SimplifySource"), simplify);
}

bool AppSettings::defaultSimplifySource()
{
    return false;
}

Language AppSettings::primaryLanguage() const
{
    const QString code = m_settings->value(QStringLiteral("Translation/PrimaryLanguage"), defaultPrimaryLanguage().toCode()).toString();
    return Language(code);
}

void AppSettings::setPrimaryLanguage(const Language &lang)
{
    m_settings->setValue(QStringLiteral("Translation/PrimaryLanguage"), lang.toCode());
}

ATranslationProvider::ProviderBackend AppSettings::translationProviderBackend() const
{
    uint8_t settingOrDefault = m_settings->value(QStringLiteral("Translation/Backend"),
                                                 static_cast<std::underlying_type_t<ATranslationProvider::ProviderBackend>>(defaultTranslationProviderBackend()))
                                   .toUInt();
    return ATranslationProvider::ProviderBackend(settingOrDefault);
}
void AppSettings::setTranslationProviderBackend(ATranslationProvider::ProviderBackend newBackend)
{
    m_settings->setValue(QStringLiteral("Translation/Backend"), static_cast<uint8_t>(newBackend));
}
ATTSProvider::ProviderBackend AppSettings::ttsProviderBackend() const
{
    uint8_t settingOrDefault = m_settings->value(QStringLiteral("TTS/Backend"), static_cast<std::underlying_type_t<ATTSProvider::ProviderBackend>>(defaultTTSProviderBackend()))
                                   .toUInt();
    return ATTSProvider::ProviderBackend(settingOrDefault);
}
void AppSettings::setTTSProviderBackend(ATTSProvider::ProviderBackend newBackend)
{
    m_settings->setValue(QStringLiteral("TTS/Backend"), static_cast<uint8_t>(newBackend));
}
ATranslationProvider::ProviderBackend AppSettings::defaultTranslationProviderBackend() const
{
    return ATranslationProvider::ProviderBackend::Mozhi;
}
ATTSProvider::ProviderBackend AppSettings::defaultTTSProviderBackend() const
{
#ifdef WITH_PIPER_TTS
    // Piper always sounds better, even on Windows
    return ATTSProvider::ProviderBackend::Piper;
#else
    return ATTSProvider::ProviderBackend::Qt;
#endif
}

#ifdef WITH_PIPER_TTS
QByteArray AppSettings::piperVoicesPath() const
{
    return m_settings->value(QStringLiteral("TTS/PiperVoicesPath"), defaultPiperVoicesPath()).toByteArray();
}

void AppSettings::setPiperVoicesPath(const QByteArray &path)
{
    m_settings->setValue(QStringLiteral("TTS/PiperVoicesPath"), path);
}

QByteArray AppSettings::defaultPiperVoicesPath()
{
    return QByteArray();
}
#endif

Language AppSettings::defaultPrimaryLanguage()
{
    return Language::autoLanguage();
}

Language AppSettings::secondaryLanguage() const
{
    const QString code = m_settings->value(QStringLiteral("Translation/SecondaryLanguage"), defaultSecondaryLanguage().toCode()).toString();
    return Language(code);
}

void AppSettings::setSecondaryLanguage(const Language &lang)
{
    m_settings->setValue(QStringLiteral("Translation/SecondaryLanguage"), lang.toCode());
}

Language AppSettings::defaultSecondaryLanguage()
{
    return Language(QLocale(QLocale::English));
}

void AppSettings::saveCustomLanguageRegistry()
{
    // Get all registered custom languages from Language class
    const auto customLanguages = Language::getCustomLanguages();

    // Clear existing custom language settings
    m_settings->remove(QStringLiteral("CustomLanguages"));

    if (customLanguages.isEmpty()) {
        return;
    }

    // Store custom languages as a group
    m_settings->beginWriteArray(QStringLiteral("CustomLanguages"), customLanguages.size());
    int index = 0;
    for (auto it = customLanguages.constBegin(); it != customLanguages.constEnd(); ++it, ++index) {
        m_settings->setArrayIndex(index);
        const QString &code = it.key();
        const auto &data = it.value();

        m_settings->setValue(QStringLiteral("code"), code);
        m_settings->setValue(QStringLiteral("name"), data.name);
        m_settings->setValue(QStringLiteral("iso639_1"), data.iso639_1);
        m_settings->setValue(QStringLiteral("iso639_2"), data.iso639_2);
        m_settings->setValue(QStringLiteral("id"), data.id);
    }
    m_settings->endArray();
}

void AppSettings::loadCustomLanguageRegistry()
{
    const int size = m_settings->beginReadArray(QStringLiteral("CustomLanguages"));

    for (int i = 0; i < size; ++i) {
        m_settings->setArrayIndex(i);

        const QString code = m_settings->value(QStringLiteral("code")).toString();
        const QString name = m_settings->value(QStringLiteral("name")).toString();
        const QString iso639_1 = m_settings->value(QStringLiteral("iso639_1")).toString();
        const QString iso639_2 = m_settings->value(QStringLiteral("iso639_2")).toString();

        if (!code.isEmpty() && !name.isEmpty()) {
            Language::registerCustomLanguage(code, name, iso639_1, iso639_2);
        }
    }

    m_settings->endArray();
}

void AppSettings::clearCustomLanguageRegistry()
{
    m_settings->remove(QStringLiteral("CustomLanguages"));
}

void AppSettings::onCustomLanguageRegistryChanged()
{
    // Static callback to save custom language registry when it changes
    AppSettings settings;
    settings.saveCustomLanguageRegistry();
}

bool AppSettings::isForceSourceAutodetect() const
{
    return m_settings->value(QStringLiteral("Translation/ForceSourceAutodetect"), defaultForceSourceAutodetect()).toBool();
}

void AppSettings::setForceSourceAutodetect(bool force)
{
    m_settings->setValue(QStringLiteral("Translation/ForceSourceAutodetect"), force);
}

bool AppSettings::defaultForceSourceAutodetect()
{
    return true;
}

bool AppSettings::isForceTranslationAutodetect() const
{
    return m_settings->value(QStringLiteral("Translation/ForceTranslationAutodetect"), defaultForceTranslationAutodetect()).toBool();
}

void AppSettings::setForceTranslationAutodetect(bool force)
{
    m_settings->setValue(QStringLiteral("Translation/ForceTranslationAutodetect"), force);
}

bool AppSettings::defaultForceTranslationAutodetect()
{
    return true;
}

QString AppSettings::instance() const
{
    return m_settings->value(QStringLiteral("Translation/Instance")).toString();
}

void AppSettings::setInstance(const QString &url)
{
    m_settings->setValue(QStringLiteral("Translation/Instance"), url);
}

QString AppSettings::libreTranslateApiKey() const
{
    return m_settings->value(QStringLiteral("Translation/LibreTranslateApiKey")).toString();
}

void AppSettings::setLibreTranslateApiKey(const QString &apiKey)
{
    m_settings->setValue(QStringLiteral("Translation/LibreTranslateApiKey"), apiKey);
}

bool AppSettings::libreTranslateDirect() const
{
    return m_settings->value(QStringLiteral("Translation/LibreTranslateDirect")).toBool();
}

void AppSettings::setLibreTranslateDirect(bool direct)
{
    m_settings->setValue(QStringLiteral("Translation/LibreTranslateDirect"), direct);
}

// ── LocalAI backend ───────────────────────────────────────────

// Model names double as QSettings keys, and a '/' would silently create a
// nested group ("openai/gpt-oss" -> group "openai", key "gpt-oss").
static QString promptKey(const QString &model)
{
    QString key = model;
    key.replace(QLatin1Char('/'), QLatin1Char('_'));
    return key;
}

QStringList AppSettings::localProviderIds()
{
    return {QStringLiteral("ollama"), QStringLiteral("fastflowlm"), QStringLiteral("lmstudio"),
            QStringLiteral("openai_custom"), QStringLiteral("anthropic")};
}

QString AppSettings::localProviderDisplayName(const QString &id)
{
    if (id == QLatin1String("ollama")) {
        return QStringLiteral("Ollama");
    }
    if (id == QLatin1String("fastflowlm")) {
        return QStringLiteral("FastFlowLM");
    }
    if (id == QLatin1String("lmstudio")) {
        return QStringLiteral("LM Studio");
    }
    if (id == QLatin1String("openai_custom")) {
        return tr("OpenAI-compatible (custom)");
    }
    if (id == QLatin1String("anthropic")) {
        return QStringLiteral("Anthropic");
    }
    return id;
}

QString AppSettings::defaultLocalProviderUrl(const QString &id)
{
    if (id == QLatin1String("fastflowlm")) {
        return QStringLiteral("http://localhost:8082");
    }
    if (id == QLatin1String("lmstudio")) {
        return QStringLiteral("http://localhost:1234");
    }
    if (id == QLatin1String("openai_custom")) {
        return QString(); // no sensible default - user's own/cloud endpoint
    }
    if (id == QLatin1String("anthropic")) {
        return QStringLiteral("https://api.anthropic.com");
    }
    return QStringLiteral("http://localhost:11434"); // ollama
}

QString AppSettings::defaultLocalProviderModel(const QString &id)
{
    if (id == QLatin1String("ollama")) {
        return QStringLiteral("translategemma_12b_128k:latest");
    }
    return QString();
}

bool AppSettings::localProviderIsAnthropic(const QString &id)
{
    return id == QLatin1String("anthropic");
}

QString AppSettings::activeLocalProvider() const
{
    return m_settings->value(QStringLiteral("LocalAI/ActiveProvider"), QStringLiteral("ollama")).toString();
}

void AppSettings::setActiveLocalProvider(const QString &id)
{
    m_settings->setValue(QStringLiteral("LocalAI/ActiveProvider"), id);
}

QString AppSettings::localProviderUrl(const QString &id) const
{
    return m_settings->value(QStringLiteral("LocalAI/") + id + QStringLiteral("/Url"), defaultLocalProviderUrl(id)).toString();
}

void AppSettings::setLocalProviderUrl(const QString &id, const QString &url)
{
    m_settings->setValue(QStringLiteral("LocalAI/") + id + QStringLiteral("/Url"), url);
}

QString AppSettings::localProviderModel(const QString &id) const
{
    return m_settings->value(QStringLiteral("LocalAI/") + id + QStringLiteral("/Model"), defaultLocalProviderModel(id)).toString();
}

void AppSettings::setLocalProviderModel(const QString &id, const QString &model)
{
    m_settings->setValue(QStringLiteral("LocalAI/") + id + QStringLiteral("/Model"), model);
}

QStringList AppSettings::localProviderModels(const QString &id) const
{
    return m_settings->value(QStringLiteral("LocalAI/") + id + QStringLiteral("/Models")).toStringList();
}

void AppSettings::setLocalProviderModels(const QString &id, const QStringList &models)
{
    m_settings->setValue(QStringLiteral("LocalAI/") + id + QStringLiteral("/Models"), models);
}

QString AppSettings::localProviderApiKey(const QString &id) const
{
    return m_settings->value(QStringLiteral("LocalAI/") + id + QStringLiteral("/ApiKey")).toString();
}

void AppSettings::setLocalProviderApiKey(const QString &id, const QString &apiKey)
{
    m_settings->setValue(QStringLiteral("LocalAI/") + id + QStringLiteral("/ApiKey"), apiKey);
}

QString AppSettings::detectProvider() const
{
    return m_settings->value(QStringLiteral("LocalAI/DetectProvider"), QStringLiteral("ollama")).toString();
}

void AppSettings::setDetectProvider(const QString &id)
{
    m_settings->setValue(QStringLiteral("LocalAI/DetectProvider"), id);
}

QString AppSettings::detectModel() const
{
    return m_settings->value(QStringLiteral("LocalAI/DetectModel")).toString();
}

void AppSettings::setDetectModel(const QString &model)
{
    m_settings->setValue(QStringLiteral("LocalAI/DetectModel"), model);
}

int AppSettings::localAiTimeout(const QString &providerId) const
{
    return m_settings->value(QStringLiteral("LocalAI/") + providerId + QStringLiteral("/Timeout"), defaultLocalAiTimeout()).toInt();
}

void AppSettings::setLocalAiTimeout(const QString &providerId, int seconds)
{
    m_settings->setValue(QStringLiteral("LocalAI/") + providerId + QStringLiteral("/Timeout"), seconds);
}

int AppSettings::defaultLocalAiTimeout()
{
    return 300;
}

bool AppSettings::localAiDisableThinking(const QString &providerId) const
{
    return m_settings->value(QStringLiteral("LocalAI/") + providerId + QStringLiteral("/DisableThinking"), false).toBool();
}

void AppSettings::setLocalAiDisableThinking(const QString &providerId, bool disable)
{
    m_settings->setValue(QStringLiteral("LocalAI/") + providerId + QStringLiteral("/DisableThinking"), disable);
}

QString AppSettings::defaultLocalAiPrompt()
{
    return QStringLiteral(
        "You are a professional translator. Determine the source language of "
        "the text yourself, then translate it into {target_lang} ({target_code}), "
        "accurately conveying its meaning and nuances while adhering to "
        "{target_lang} grammar, vocabulary, and cultural sensitivities.\n"
        "Produce only the {target_lang} translation, without any additional "
        "explanations or commentary. Translate the following text into "
        "{target_lang}:\n\n\n{text}");
}

// The default translation prompt as it stood before the current one replaced
// it. It asserted a specific source language into every request ("You are a
// professional {source_lang} ... translate the following {source_lang} text"),
// which the model does not need and can get wrong.
//
// It has to stay named here because the settings dialog used to persist the
// prompt on every keystroke, so anyone who has merely *opened* the LocalAI page
// on a master build has this string stored under their model - and a stored
// prompt wins over the default. Left alone, upgrading would go on sending the
// old prompt while the settings page showed it as the current default.
//
// Byte-identical to this means nobody typed it: it is that auto-save, not a
// customization, so it reads back as "nothing stored". Anything else, however
// similar, is the user's own text and is returned untouched.
static QString supersededSourceLangPrompt()
{
    return QStringLiteral(
        "You are a professional {source_lang} ({source_code}) to {target_lang} ({target_code}) translator. "
        "Your goal is to accurately convey the meaning and nuances of the original {source_lang} text "
        "while adhering to {target_lang} grammar, vocabulary, and cultural sensitivities.\n"
        "Produce only the {target_lang} translation, without any additional explanations or commentary. "
        "Please translate the following {source_lang} text into {target_lang}:\n\n\n{text}");
}

QString AppSettings::localAiPrompt(const QString &model) const
{
    const QString stored = m_settings->value(QStringLiteral("LocalAI/Prompts/") + promptKey(model)).toString();
    if (stored.isEmpty() || stored == supersededSourceLangPrompt()) {
        return defaultLocalAiPrompt();
    }
    return stored;
}

void AppSettings::setLocalAiPrompt(const QString &model, const QString &prompt)
{
    m_settings->setValue(QStringLiteral("LocalAI/Prompts/") + promptKey(model), prompt);
}

QNetworkProxy::ProxyType AppSettings::proxyType() const
{
    return static_cast<QNetworkProxy::ProxyType>(m_settings->value(QStringLiteral("Connection/ProxyType"), defaultProxyType()).toInt());
}

void AppSettings::setProxyType(QNetworkProxy::ProxyType type)
{
    m_settings->setValue(QStringLiteral("Connection/ProxyType"), type);
}

QNetworkProxy::ProxyType AppSettings::defaultProxyType()
{
    return QNetworkProxy::DefaultProxy;
}

QString AppSettings::proxyHost() const
{
    return m_settings->value(QStringLiteral("Connection/ProxyHost"), defaultProxyHost()).toString();
}

void AppSettings::setProxyHost(const QString &hostName)
{
    m_settings->setValue(QStringLiteral("Connection/ProxyHost"), hostName);
}

QString AppSettings::defaultProxyHost()
{
    return {};
}

quint16 AppSettings::proxyPort() const
{
    return m_settings->value(QStringLiteral("Connection/ProxyPort"), defaultProxyPort()).value<quint16>();
}

void AppSettings::setProxyPort(quint16 port)
{
    m_settings->setValue(QStringLiteral("Connection/ProxyPort"), port);
}

quint16 AppSettings::defaultProxyPort()
{
    return 8080;
}

bool AppSettings::isProxyAuthEnabled() const
{
    return m_settings->value(QStringLiteral("Connection/ProxyAuthEnabled"), defaultProxyAuthEnabled()).toBool();
}

void AppSettings::setProxyAuthEnabled(bool enabled)
{
    m_settings->setValue(QStringLiteral("Connection/ProxyAuthEnabled"), enabled);
}

bool AppSettings::defaultProxyAuthEnabled()
{
    return false;
}

QString AppSettings::proxyUsername() const
{
    return m_settings->value(QStringLiteral("Connection/ProxyUsername"), defaultProxyUsername()).toString();
}

void AppSettings::setProxyUsername(const QString &username)
{
    m_settings->setValue(QStringLiteral("Connection/ProxyUsername"), username);
}

QString AppSettings::defaultProxyUsername()
{
    return {};
}

QString AppSettings::proxyPassword() const
{
    return m_settings->value(QStringLiteral("Connection/ProxyPassword"), defaultProxyPassword()).toString();
}

void AppSettings::setProxyPassword(const QString &password)
{
    m_settings->setValue(QStringLiteral("Connection/ProxyPassword"), password);
}

QString AppSettings::defaultProxyPassword()
{
    return {};
}

bool AppSettings::isGlobalShortuctsEnabled() const
{
    return m_settings->value(QStringLiteral("Shortcuts/GlobalShortcutsEnabled"), defaultGlobalShortcutsEnabled()).toBool();
}

void AppSettings::setGlobalShortcutsEnabled(bool enabled)
{
    m_settings->setValue(QStringLiteral("Shortcuts/GlobalShortcutsEnabled"), enabled);
}

bool AppSettings::defaultGlobalShortcutsEnabled()
{
    return QHotkey::isPlatformSupported();
}

QKeySequence AppSettings::translateSelectionShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/TranslateSelection"), defaultTranslateSelectionShortcut()).value<QKeySequence>();
}

void AppSettings::setTranslateSelectionShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/TranslateSelection"), shortcut);
}

QKeySequence AppSettings::defaultTranslateSelectionShortcut()
{
    return {QStringLiteral("Ctrl+Alt+E")};
}

QKeySequence AppSettings::speakSelectionShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/SpeakSelection"), defaultSpeakSelectionShortcut()).value<QKeySequence>();
}

void AppSettings::setSpeakSelectionShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/SpeakSelection"), shortcut);
}

QKeySequence AppSettings::defaultSpeakSelectionShortcut()
{
    return {QStringLiteral("Ctrl+Alt+S")};
}

QKeySequence AppSettings::speakTranslatedSelectionShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/SpeakTranslatedSelection"), defaultSpeakTranslatedSelectionShortcut()).value<QKeySequence>();
}

void AppSettings::setSpeakTranslatedSelectionShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/SpeakTranslatedSelection"), shortcut);
}

QKeySequence AppSettings::defaultSpeakTranslatedSelectionShortcut()
{
    return {QStringLiteral("Ctrl+Alt+F")};
}

QKeySequence AppSettings::stopSpeakingShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/StopSelection"), defaultStopSpeakingShortcut()).value<QKeySequence>();
}

void AppSettings::setStopSpeakingShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/StopSelection"), shortcut);
}

QKeySequence AppSettings::defaultStopSpeakingShortcut()
{
    return {QStringLiteral("Ctrl+Alt+G")};
}

QKeySequence AppSettings::playPauseSpeakingShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/PlayPauseSpeakingSelection"), defaultStopSpeakingShortcut()).value<QKeySequence>();
}

void AppSettings::setPlayPauseSpeakingShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/PlayPauseSpeakingSelection"), shortcut);
}

QKeySequence AppSettings::defaultPlayPauseSpeakingShortcut()
{
    return {QStringLiteral("Ctrl+Alt+D")};
}

QKeySequence AppSettings::showMainWindowShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/ShowMainWindow"), defaultShowMainWindowShortcut()).value<QKeySequence>();
}

void AppSettings::setShowMainWindowShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/ShowMainWindow"), shortcut);
}

QKeySequence AppSettings::defaultShowMainWindowShortcut()
{
    return {QStringLiteral("Ctrl+Alt+C")};
}

QKeySequence AppSettings::copyTranslatedSelectionShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/CopyTranslatedSelection"), defaultCopyTranslatedSelectionShortcut()).toString();
}

void AppSettings::setCopyTranslatedSelectionShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/CopyTranslatedSelection"), shortcut);
}

QKeySequence AppSettings::defaultCopyTranslatedSelectionShortcut()
{
    return {};
}

QKeySequence AppSettings::recognizeScreenAreaShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/RecognizeScreenAreaShortcut"), defaultRecognizeScreenAreaShortcut()).toString();
}

void AppSettings::setRecognizeScreenAreaShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/RecognizeScreenAreaShortcut"), shortcut);
}

QKeySequence AppSettings::defaultRecognizeScreenAreaShortcut()
{
    return {QStringLiteral("Ctrl+Alt+I")};
}

QKeySequence AppSettings::translateScreenAreaShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/TranslateScreenAreaShortcut"), defaultTranslateScreenAreaShortcut()).toString();
}

void AppSettings::setTranslateScreenAreaShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/TranslateScreenAreaShortcut"), shortcut);
}

QKeySequence AppSettings::defaultTranslateScreenAreaShortcut()
{
    return {QStringLiteral("Ctrl+Alt+O")};
}

QKeySequence AppSettings::delayedRecognizeScreenAreaShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/DelayedRecognizeScreenAreaShortcut"), defaultDelayedRecognizeScreenAreaShortcut()).toString();
}

void AppSettings::setDelayedRecognizeScreenAreaShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/DelayedRecognizeScreenAreaShortcut"), shortcut);
}

QKeySequence AppSettings::defaultDelayedRecognizeScreenAreaShortcut()
{
    return {};
}

QKeySequence AppSettings::delayedTranslateScreenAreaShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/DelayedTranslateScreenAreaShortcut"), defaultDelayedTranslateScreenAreaShortcut()).toString();
}

void AppSettings::setDelayedTranslateScreenAreaShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/DelayedTranslateScreenAreaShortcut"), shortcut);
}

QKeySequence AppSettings::defaultDelayedTranslateScreenAreaShortcut()
{
    return {};
}

QKeySequence AppSettings::translateShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/Translate"), defaultTranslateShortcut()).value<QKeySequence>();
}

void AppSettings::setTranslateShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/Translate"), shortcut);
}

QKeySequence AppSettings::defaultTranslateShortcut()
{
    return {QStringLiteral("Ctrl+Return")};
}

QKeySequence AppSettings::swapShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/Swap"), defaultSwapShortcut()).value<QKeySequence>();
}

void AppSettings::setSwapShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/Swap"), shortcut);
}

QKeySequence AppSettings::defaultSwapShortcut()
{
    return {QStringLiteral("Ctrl+R")};
}

QKeySequence AppSettings::closeWindowShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/CloseWindow"), defaultCloseWindowShortcut()).value<QKeySequence>();
}

void AppSettings::setCloseWindowShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/CloseWindow"), shortcut);
}

QKeySequence AppSettings::defaultCloseWindowShortcut()
{
    return {QStringLiteral("Ctrl+Q")};
}

QKeySequence AppSettings::speakSourceShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/SpeakSource"), defaultSpeakSourceShortcut()).value<QKeySequence>();
}

void AppSettings::setSpeakSourceShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/SpeakSource"), shortcut);
}

QKeySequence AppSettings::defaultSpeakSourceShortcut()
{
    return {QStringLiteral("Ctrl+S")};
}

QKeySequence AppSettings::speakTranslationShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/SpeakTranslation"), defaultSpeakTranslationShortcut()).value<QKeySequence>();
}

void AppSettings::setSpeakTranslationShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/SpeakTranslation"), shortcut);
}

QKeySequence AppSettings::defaultSpeakTranslationShortcut()
{
    return {QStringLiteral("Ctrl+Shift+S")};
}

QKeySequence AppSettings::copyTranslationShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/CopyTranslation"), defaultCopyTranslationShortcut()).value<QKeySequence>();
}

void AppSettings::setCopyTranslationShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/CopyTranslation"), shortcut);
}

QKeySequence AppSettings::defaultCopyTranslationShortcut()
{
    return {QStringLiteral("Ctrl+Shift+C")};
}

QKeySequence AppSettings::toggleOcrNegateShortcut() const
{
    return m_settings->value(QStringLiteral("Shortcuts/ToggleOcrNegate")).value<QKeySequence>();
}

void AppSettings::setToggleOcrNegateShortcut(const QKeySequence &shortcut)
{
    m_settings->setValue(QStringLiteral("Shortcuts/ToggleOcrNegate"), shortcut);
}

bool AppSettings::isConvertLineBreaks() const
{
    return m_settings->value(QStringLiteral("OCR/ConvertLineBreaks"), defaultConvertLineBreaks()).toBool();
}

void AppSettings::setConvertLineBreaks(bool convert)
{
    m_settings->setValue(QStringLiteral("OCR/ConvertLineBreaks"), convert);
}

bool AppSettings::defaultConvertLineBreaks()
{
    return true;
}

AppSettings::OcrEngine AppSettings::ocrEngine() const
{
    return static_cast<OcrEngine>(m_settings->value(QStringLiteral("OCR/Engine"), static_cast<int>(defaultOcrEngine())).toInt());
}

void AppSettings::setOcrEngine(OcrEngine engine)
{
    m_settings->setValue(QStringLiteral("OCR/Engine"), static_cast<int>(engine));
}

AppSettings::OcrEngine AppSettings::defaultOcrEngine()
{
    return OcrEngine::Tesseract;
}

QString AppSettings::ocrLlmProvider() const
{
    return m_settings->value(QStringLiteral("OcrLlm/Provider"), QStringLiteral("ollama")).toString();
}

void AppSettings::setOcrLlmProvider(const QString &providerId)
{
    m_settings->setValue(QStringLiteral("OcrLlm/Provider"), providerId);
}

QString AppSettings::ocrLlmUrl(const QString &providerId) const
{
    return m_settings->value(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/Url"), defaultLocalProviderUrl(providerId)).toString();
}

void AppSettings::setOcrLlmUrl(const QString &providerId, const QString &url)
{
    m_settings->setValue(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/Url"), url);
}

QString AppSettings::ocrLlmApiKey(const QString &providerId) const
{
    return m_settings->value(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/ApiKey")).toString();
}

void AppSettings::setOcrLlmApiKey(const QString &providerId, const QString &apiKey)
{
    m_settings->setValue(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/ApiKey"), apiKey);
}

QString AppSettings::ocrLlmModel(const QString &providerId) const
{
    return m_settings->value(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/Model")).toString();
}

void AppSettings::setOcrLlmModel(const QString &providerId, const QString &model)
{
    m_settings->setValue(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/Model"), model);
}

QStringList AppSettings::ocrLlmModels(const QString &providerId) const
{
    return m_settings->value(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/Models")).toStringList();
}

void AppSettings::setOcrLlmModels(const QString &providerId, const QStringList &models)
{
    m_settings->setValue(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/Models"), models);
}

QStringList AppSettings::ocrLlmVisionModels(const QString &providerId) const
{
    return m_settings->value(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/VisionModels")).toStringList();
}

void AppSettings::setOcrLlmVisionModels(const QString &providerId, const QStringList &models)
{
    m_settings->setValue(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/VisionModels"), models);
}

QString AppSettings::ocrLlmPrompt(const QString &model) const
{
    return m_settings->value(QStringLiteral("OcrLlm/Prompts/") + promptKey(model)).toString();
}

void AppSettings::setOcrLlmPrompt(const QString &model, const QString &prompt)
{
    m_settings->setValue(QStringLiteral("OcrLlm/Prompts/") + promptKey(model), prompt);
}

int AppSettings::ocrLlmTimeout(const QString &providerId) const
{
    return m_settings->value(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/Timeout"), defaultLocalAiTimeout()).toInt();
}

void AppSettings::setOcrLlmTimeout(const QString &providerId, int seconds)
{
    m_settings->setValue(QStringLiteral("OcrLlm/") + providerId + QStringLiteral("/Timeout"), seconds);
}

QByteArray AppSettings::ocrLanguagesPath() const
{
    return m_settings->value(QStringLiteral("OCR/LanguagesPath"), defaultOcrLanguagesPath()).toByteArray();
}

void AppSettings::setOcrLanguagesPath(const QByteArray &path)
{
    m_settings->setValue(QStringLiteral("OCR/LanguagesPath"), path);
}

QByteArray AppSettings::defaultOcrLanguagesPath()
{
    return {};
}

QByteArray AppSettings::ocrLanguagesString() const
{
    return m_settings->value(QStringLiteral("OCR/Languages"), defaultOcrLanguagesString()).toByteArray();
}

void AppSettings::setOcrLanguagesString(const QByteArray &string)
{
    m_settings->setValue(QStringLiteral("OCR/Languages"), string);
}

QByteArray AppSettings::defaultOcrLanguagesString()
{
    return {};
}

bool AppSettings::isShowMagnifier() const
{
    return m_settings->value(QStringLiteral("OCR/ShowMagnifier"), defaultShowMagnifier()).toBool();
}

void AppSettings::setShowMagnifier(bool show)
{
    m_settings->setValue(QStringLiteral("OCR/ShowMagnifier"), show);
}

bool AppSettings::defaultShowMagnifier()
{
    return false;
}

QMap<QString, QVariant> AppSettings::tesseractParameters() const
{
    QMap<QString, QVariant> parameters;
    m_settings->beginGroup("Tesseract");
    for (const QString &key : m_settings->childKeys())
        parameters.insert(key, m_settings->value(key));
    m_settings->endGroup();
    return parameters;
}

void AppSettings::setTesseractParameters(const QMap<QString, QVariant> &parameters)
{
    m_settings->beginGroup("Tesseract");
    m_settings->remove({}); // Remove all keys in current group
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
        m_settings->setValue(it.key(), it.value());
    m_settings->endGroup();
}

QMap<QString, QVariant> AppSettings::defaultTesseractParameters()
{
    return {};
}

AppSettings::RegionRememberType AppSettings::regionRememberType() const
{
    return m_settings->value(QStringLiteral("OCR/RegionRememberType"), defaultRegionRememberType()).value<RegionRememberType>();
}

void AppSettings::setRegionRememberType(RegionRememberType type)
{
    m_settings->setValue(QStringLiteral("OCR/RegionRememberType"), type);
    if (type != RememberAlways)
        m_settings->remove(QStringLiteral("OCR/RememberedCropRegion"));
}

AppSettings::RegionRememberType AppSettings::defaultRegionRememberType()
{
    return NeverRemember;
}

int AppSettings::captureDelay() const
{
    return m_settings->value(QStringLiteral("OCR/CaptureDelay"), defaultCaptureDelay()).toInt();
}

void AppSettings::setCaptureDelay(int ms)
{
    m_settings->setValue(QStringLiteral("OCR/CaptureDelay"), ms);
}

int AppSettings::defaultCaptureDelay()
{
    return 1000;
}

bool AppSettings::isConfirmOnRelease() const
{
    return m_settings->value(QStringLiteral("OCR/ConfirmOnRelease"), defaultConfirmOnRelease()).toBool();
}

void AppSettings::setConfirmOnRelease(bool capture)
{
    m_settings->setValue(QStringLiteral("OCR/ConfirmOnRelease"), capture);
}

bool AppSettings::defaultConfirmOnRelease()
{
    return false;
}

bool AppSettings::isApplyLightMask() const
{
    return m_settings->value(QStringLiteral("OCR/ApplyLightMask"), defaultApplyLightMask()).toBool();
}

void AppSettings::setApplyLightMask(bool use)
{
    m_settings->setValue(QStringLiteral("OCR/ApplyLightMask"), use);
}

bool AppSettings::defaultApplyLightMask()
{
    return true;
}

QRect AppSettings::cropRegion() const
{
    return m_settings->value(QStringLiteral("OCR/CropRegion"), defaultRegionRememberType()).toRect();
}

void AppSettings::setCropRegion(QRect rect)
{
    m_settings->setValue(QStringLiteral("OCR/CropRegion"), rect);
}

bool AppSettings::isOcrNegate() const
{
    return m_settings->value(QStringLiteral("OCR/Negate"), defaultOcrNegate()).toBool();
}

void AppSettings::setOcrNegate(bool negate)
{
    m_settings->setValue(QStringLiteral("OCR/Negate"), negate);
}

bool AppSettings::defaultOcrNegate()
{
    return false;
}

bool AppSettings::toggleOcrNegate()
{
    const bool inverted = !isOcrNegate();
    setOcrNegate(inverted);
    return inverted;
}

QVector<Language> AppSettings::languages(LanguageButtonsType type) const
{
    const auto typeEnum = QMetaEnum::fromType<LanguageButtonsType>();
    const QStringList languageCodes = m_settings->value(QStringLiteral("Buttons/%1").arg(typeEnum.valueToKey(type))).toStringList();

    QVector<Language> languages;
    languages.reserve(languageCodes.size());
    for (const QString &langCode : languageCodes) {
        const Language language = Language(langCode);
        if (language.isValid())
            languages.append(language);
        else
            qWarning() << tr("Unknown language code: %1").arg(langCode);
    }

    return languages;
}

void AppSettings::setLanguages(LanguageButtonsType type, const QVector<Language> &languages)
{
    QStringList langCodes;
    langCodes.reserve(languages.size());
    for (const Language &language : languages) {
        if (language.hasQLocaleEquivalent())
            langCodes.append(language.toQLocale().bcp47Name()); // Use full BCP47 code to preserve dialect info
        else
            langCodes.append(language.toCode()); // Use custom code for non-QLocale languages
    }

    const auto typeEnum = QMetaEnum::fromType<LanguageButtonsType>();
    m_settings->setValue(QStringLiteral("Buttons/%1").arg(typeEnum.valueToKey(type)), langCodes);
}

int AppSettings::checkedButton(LanguageButtonsType type) const
{
    const QMetaEnum typeEnum = QMetaEnum::fromType<LanguageButtonsType>();

    return m_settings->value(QStringLiteral("Buttons/Checked%1").arg(typeEnum.valueToKey(type)), LanguageButtonsWidget::autoButtonId()).toInt();
}

void AppSettings::setCheckedButton(LanguageButtonsType type, int id)
{
    const QMetaEnum typeEnum = QMetaEnum::fromType<LanguageButtonsType>();

    m_settings->setValue(QStringLiteral("Buttons/Checked%1").arg(typeEnum.valueToKey(type)), id);
}

bool AppSettings::isShowPrivacyPopup() const
{
    return m_settings->value(QStringLiteral("MainWindow/ShowPrivacyPopup"), true).toBool();
}

void AppSettings::setShowPrivacyPopup(bool show)
{
    m_settings->setValue(QStringLiteral("MainWindow/ShowPrivacyPopup"), show);
}

QByteArray AppSettings::mainWindowGeometry() const
{
    return m_settings->value(QStringLiteral("MainWindow/WindowGeometry")).toByteArray();
}

void AppSettings::setMainWindowGeometry(const QByteArray &geometry)
{
    m_settings->setValue(QStringLiteral("MainWindow/WindowGeometry"), geometry);
}

bool AppSettings::isAutoTranslateEnabled() const
{
    return m_settings->value(QStringLiteral("MainWindow/AutoTranslate"), false).toBool();
}

void AppSettings::setAutoTranslateEnabled(bool enable)
{
    m_settings->setValue(QStringLiteral("MainWindow/AutoTranslate"), enable);
}

OnlineTranslator::Engine AppSettings::currentEngine() const
{
    return m_settings->value(QStringLiteral("MainWindow/CurrentEngine"), OnlineTranslator::Google).value<OnlineTranslator::Engine>();
}

void AppSettings::setCurrentEngine(OnlineTranslator::Engine currentEngine)
{
    m_settings->setValue(QStringLiteral("MainWindow/CurrentEngine"), currentEngine);
}

#ifdef WITH_ONNX_RUNTIME_DYNAMIC
bool AppSettings::isPiperTelemetryNotificationShown() const
{
    return m_settings->value(QStringLiteral("Piper/TelemetryNotificationShown"), false).toBool();
}

void AppSettings::setPiperTelemetryNotificationShown(bool shown)
{
    m_settings->setValue(QStringLiteral("Piper/TelemetryNotificationShown"), shown);
}
#endif
