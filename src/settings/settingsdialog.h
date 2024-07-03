/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "onlinetranslator/onlinetts.h"

#include <QDialog>

class MainWindow;
class AbstractAutostartManager;
class OnlineTranslator;
class QMediaPlayer;
class QMediaPlaylist;
class ShortcutItem;
#ifdef WITH_PORTABLE_MODE
class QCheckBox;
#endif

namespace Ui
{
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(SettingsDialog)

public:
    explicit SettingsDialog(MainWindow *parent = nullptr);
    ~SettingsDialog() override;

public slots:
    void accept() override;

private slots:
    void setCurrentPage(int index);

    void onProxyTypeChanged(int type);
    void onWindowModeChanged(int mode);

    void onTrayIconTypeChanged(int type);
    void selectCustomTrayIcon();
    void setCustomTrayIconPreview(const QString &iconPath);

    void selectOcrLanguagesPath();
    void onOcrLanguagesPathChanged(const QString &path);
    void onTesseractParametersCurrentItemChanged();

    void saveYandexEngineVoice(int voice);
    void saveYandexEngineEmotion(int emotion);
    void detectYandexTextLanguage();
    void speakYandexTestText();

    void onGoogleLanguageSelectionChanged(int languageIndex);
    void saveGoogleEngineRegion(int region);
    void detectGoogleTextLanguage();
    void speakGoogleTestText();

    void loadShortcut(ShortcutItem *item);
    void updateAcceptButton();
    void acceptCurrentShortcut();
    void clearCurrentShortcut();
    void resetCurrentShortcut();
    void resetAllShortcuts();

    void restoreDefaults();

private:
    void addLocale(const QLocale &locale);
    void activateCompactMode();
    void loadSettings();

    void detectTestTextLanguage(OnlineTranslator &translator, OnlineTranslator::Engine engine);
    void speakTestText(OnlineTranslator &translator, OnlineTranslator::Engine engine);

    Ui::SettingsDialog *ui;

    // Manage platform-dependant autostart
    AbstractAutostartManager *m_autostartManager;

    // Test voice
    OnlineTranslator *m_yandexTranslator;
    OnlineTranslator *m_googleTranslator;

#ifdef WITH_PORTABLE_MODE
    QCheckBox *m_portableCheckbox;
#endif
};

#endif // SETTINGSDIALOG_H
