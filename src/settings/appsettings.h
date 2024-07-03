/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include "onlinetranslator/onlinetts.h"

#include <QCoreApplication>
#include <QLocale>
#include <QNetworkProxy>

class QTranslator;
class QSettings;

class AppSettings : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AppSettings)

public:
    enum LanguageFormat {
        FullName,
        IsoCode
    };
    Q_ENUM(LanguageFormat)

    enum LanguageButtonsType {
        Source,
        Translation
    };
    Q_ENUM(LanguageButtonsType)

    enum WindowMode {
        PopupWindow,
        MainWindow,
        Notification
    };
    Q_ENUM(WindowMode)

    enum RegionRememberType {
        NeverRemember,
        RememberLast,
        RememberAlways
    };
    Q_ENUM(RegionRememberType)

    enum IconType {
        DefaultIcon,
        LightIcon,
        DarkIcon,
        CustomIcon
    };
    Q_ENUM(IconType)

    explicit AppSettings(QObject *parent = nullptr);

    // General settings
    void setupLocalization() const;
    QLocale locale() const;
    void setLocale(const QLocale &locale);
    static void applyLocale(const QLocale &locale);
    static bool loadLocale(const QString &localeDirName);
    static QLocale defaultLocale();

    Qt::ScreenOrientation mainWindowOrientation() const;
    void setMainWindowOrientation(Qt::ScreenOrientation mode);
    static Qt::ScreenOrientation defaultMainWindowOrientation();

    WindowMode windowMode() const;
    void setWindowMode(WindowMode mode);
    static WindowMode defaultWindowMode();

    int translationNotificationTimeout() const;
    void setTranslationNotificationTimeout(int timeout);
    static int defaultTranslationNotificationTimeout();

    int popupWindowTimeout() const;
    void setPopupWindowTimeout(int timeout);
    static int defaultPopupWindowTimeout();

    bool isShowTrayIcon() const;
    void setShowTrayIcon(bool visible);
    static bool defaultShowTrayIcon();

    bool isStartMinimized() const;
    void setStartMinimized(bool minimized);
    static bool defaultStartMinimized();

    // Used by only by Flatpak to store information about if autostart was enabled
    bool isAutostartEnabled() const;
    void setAutostartEnabled(bool enabled);
    static bool defaultAutostartEnabled();

#ifdef WITH_PORTABLE_MODE
    bool isPortableModeEnabled() const;
    static void setPortableModeEnabled(bool enabled);
    static QString portableConfigName();
#endif

    // Interface settings
    QFont font() const;
    void setFont(const QFont &font);

    double popupOpacity() const;
    void setPopupOpacity(double opacity);
    static double defaultPopupOpacity();

    int popupHeight() const;
    void setPopupHeight(int height);
    static int defaultPopupHeight();

    int popupWidth() const;
    void setPopupWidth(int width);
    static int defaultPopupWidth();

    LanguageFormat popupLanguageFormat() const;
    void setPopupLanguageFormat(LanguageFormat style);
    static LanguageFormat defaultPopupLanguageFormat();

    LanguageFormat mainWindowLanguageFormat() const;
    void setMainWindowLanguageFormat(LanguageFormat style);
    static LanguageFormat defaultMainWindowLanguageFormat();

    IconType trayIconType() const;
    void setTrayIconType(IconType type);
    static IconType defaultTrayIconType();

    QString customIconPath() const;
    void setCustomIconPath(const QString &path);
    static QString defaultCustomIconPath();

    // Translation settings
    bool isSourceTranslitEnabled() const;
    void setSourceTranslitEnabled(bool enable);
    static bool defaultSourceTranslitEnabled();

    bool isTranslationTranslitEnabled() const;
    void setTranslationTranslitEnabled(bool enable);
    static bool defaultTranslationTranslitEnabled();

    bool isSourceTranscriptionEnabled() const;
    void setSourceTranscriptionEnabled(bool enable);
    static bool defaultSourceTranscriptionEnabled();

    bool isTranslationOptionsEnabled() const;
    void setTranslationOptionsEnabled(bool enable);
    static bool defaultTranslationOptionsEnabled();

    bool isExamplesEnabled() const;
    void setExamplesEnabled(bool enable);
    static bool defaultExamplesEnabled();

    bool isSimplifySource() const;
    void setSimplifySource(bool simplify);
    static bool defaultSimplifySource();

    OnlineTranslator::Language primaryLanguage() const;
    void setPrimaryLanguage(OnlineTranslator::Language lang);
    static OnlineTranslator::Language defaultPrimaryLanguage();

    OnlineTranslator::Language secondaryLanguage() const;
    void setSecondaryLanguage(OnlineTranslator::Language lang);
    static OnlineTranslator::Language defaultSecondaryLanguage();

    bool isForceSourceAutodetect() const;
    void setForceSourceAutodetect(bool force);
    static bool defaultForceSourceAutodetect();

    bool isForceTranslationAutodetect() const;
    void setForceTranslationAutodetect(bool force);
    static bool defaultForceTranslationAutodetect();

    QString engineUrl(OnlineTranslator::Engine engine) const;
    void setEngineUrl(OnlineTranslator::Engine engine, const QString &url);
    static QString defaultEngineUrl(OnlineTranslator::Engine engine);

    QByteArray engineApiKey(OnlineTranslator::Engine engine) const;
    void setEngineApiKey(OnlineTranslator::Engine engine, const QByteArray &apiKey);
    static QByteArray defaultEngineApiKey(OnlineTranslator::Engine engine);

    // Speech synthesis settings
    OnlineTts::Voice voice(OnlineTranslator::Engine engine) const;
    void setVoice(OnlineTranslator::Engine engine, OnlineTts::Voice voice);
    static OnlineTts::Voice defaultVoice(OnlineTranslator::Engine engine);

    OnlineTts::Emotion emotion(OnlineTranslator::Engine engine) const;
    void setEmotion(OnlineTranslator::Engine engine, OnlineTts::Emotion emotion);
    static OnlineTts::Emotion defaultEmotion(OnlineTranslator::Engine engine);

    QMap<OnlineTranslator::Language, QLocale::Country> regions(OnlineTranslator::Engine engine) const;
    void setRegions(OnlineTranslator::Engine engine, const QMap<OnlineTranslator::Language, QLocale::Country> &regions);
    static QMap<OnlineTranslator::Language, QLocale::Country> defaultRegions(OnlineTranslator::Engine engine);

    // Connection settings
    QNetworkProxy::ProxyType proxyType() const;
    void setProxyType(QNetworkProxy::ProxyType type);
    static QNetworkProxy::ProxyType defaultProxyType();

    QString proxyHost() const;
    void setProxyHost(const QString &hostName);
    static QString defaultProxyHost();

    quint16 proxyPort() const;
    void setProxyPort(quint16 port);
    static quint16 defaultProxyPort();

    bool isProxyAuthEnabled() const;
    void setProxyAuthEnabled(bool enabled);
    static bool defaultProxyAuthEnabled();

    QString proxyUsername() const;
    void setProxyUsername(const QString &username);
    static QString defaultProxyUsername();

    QString proxyPassword() const;
    void setProxyPassword(const QString &password);
    static QString defaultProxyPassword();

    // Global shortcuts
    bool isGlobalShortuctsEnabled() const;
    void setGlobalShortcutsEnabled(bool enabled);
    static bool defaultGlobalShortcutsEnabled();

    QKeySequence translateSelectionShortcut() const;
    void setTranslateSelectionShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultTranslateSelectionShortcut();

    QKeySequence speakSelectionShortcut() const;
    void setSpeakSelectionShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultSpeakSelectionShortcut();

    QKeySequence speakTranslatedSelectionShortcut() const;
    void setSpeakTranslatedSelectionShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultSpeakTranslatedSelectionShortcut();

    QKeySequence stopSpeakingShortcut() const;
    void setStopSpeakingShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultStopSpeakingShortcut();

    QKeySequence playPauseSpeakingShortcut() const;
    void setPlayPauseSpeakingShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultPlayPauseSpeakingShortcut();

    QKeySequence showMainWindowShortcut() const;
    void setShowMainWindowShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultShowMainWindowShortcut();

    QKeySequence copyTranslatedSelectionShortcut() const;
    void setCopyTranslatedSelectionShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultCopyTranslatedSelectionShortcut();

    QKeySequence recognizeScreenAreaShortcut() const;
    void setRecognizeScreenAreaShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultRecognizeScreenAreaShortcut();

    QKeySequence translateScreenAreaShortcut() const;
    void setTranslateScreenAreaShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultTranslateScreenAreaShortcut();

    QKeySequence delayedRecognizeScreenAreaShortcut() const;
    void setDelayedRecognizeScreenAreaShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultDelayedRecognizeScreenAreaShortcut();

    QKeySequence delayedTranslateScreenAreaShortcut() const;
    void setDelayedTranslateScreenAreaShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultDelayedTranslateScreenAreaShortcut();

    // Window shortcuts
    QKeySequence translateShortcut() const;
    void setTranslateShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultTranslateShortcut();

    QKeySequence swapShortcut() const;
    void setSwapShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultSwapShortcut();

    QKeySequence closeWindowShortcut() const;
    void setCloseWindowShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultCloseWindowShortcut();

    QKeySequence speakSourceShortcut() const;
    void setSpeakSourceShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultSpeakSourceShortcut();

    QKeySequence speakTranslationShortcut() const;
    void setSpeakTranslationShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultSpeakTranslationShortcut();

    QKeySequence copyTranslationShortcut() const;
    void setCopyTranslationShortcut(const QKeySequence &shortcut);
    static QKeySequence defaultCopyTranslationShortcut();

    // OCR settings
    bool isConvertLineBreaks() const;
    void setConvertLineBreaks(bool convert);
    static bool defaultConvertLineBreaks();

    QByteArray ocrLanguagesPath() const;
    void setOcrLanguagesPath(const QByteArray &path);
    static QByteArray defaultOcrLanguagesPath();

    QByteArray ocrLanguagesString() const;
    void setOcrLanguagesString(const QByteArray &string);
    static QByteArray defaultOcrLanguagesString();

    QMap<QString, QVariant> tesseractParameters() const;
    void setTesseractParameters(const QMap<QString, QVariant> &parameters);
    static QMap<QString, QVariant> defaultTesseractParameters();

    RegionRememberType regionRememberType() const;
    void setRegionRememberType(RegionRememberType type);
    static RegionRememberType defaultRegionRememberType();

    int captureDelay() const;
    void setCaptureDelay(int ms);
    static int defaultCaptureDelay();

    bool isShowMagnifier() const;
    void setShowMagnifier(bool show);
    static bool defaultShowMagnifier();

    bool isConfirmOnRelease() const;
    void setConfirmOnRelease(bool capture);
    static bool defaultConfirmOnRelease();

    bool isApplyLightMask() const;
    void setApplyLightMask(bool use);
    static bool defaultApplyLightMask();

    QRect cropRegion() const;
    void setCropRegion(QRect rect);

    // Buttons
    QVector<OnlineTranslator::Language> languages(LanguageButtonsType type) const;
    void setLanguages(LanguageButtonsType type, const QVector<OnlineTranslator::Language> &languages);

    int checkedButton(LanguageButtonsType type) const;
    void setCheckedButton(LanguageButtonsType type, int id);

    // Main window settings
    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray &geometry);

    bool isAutoTranslateEnabled() const;
    void setAutoTranslateEnabled(bool enable);

    OnlineTranslator::Engine currentEngine() const;
    void setCurrentEngine(OnlineTranslator::Engine currentEngine);

private:
    static QTranslator s_appTranslator;
    static QTranslator s_qtTranslator; // Qt library translations

    QSettings *m_settings;
};

#endif // APPSETTINGS_H
